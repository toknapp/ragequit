#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/socket.h>

#define LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))

static int setup_netlink(int flags)
{
    int fd = socket(AF_NETLINK, SOCK_RAW | flags, NETLINK_GENERIC);
    if(fd < 0) perror("socket()"), exit(1);

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, 0 };

    int r = bind(fd, (struct sockaddr*)&sa, sizeof(sa));
    if(r != 0) perror("bind()"), exit(1);

    return fd;
}

struct outbound_msg {
    uint16_t type; uint32_t flags;
    void* buf; size_t len;

    struct outbound_msg* next;
};

static struct outbound_msg* outbound_queue;

static void enqueue_outbound(uint16_t type, uint32_t flags,
                             const void* buf, size_t len)
{
    struct outbound_msg* m = calloc(sizeof(*m), 1);
    if(!m) err(1, "calloc");

    m->type = type; m->flags = flags; m->len = len;
    m->buf = malloc(len); if(!m->buf) err(1, "malloc");
    memcpy(m->buf, buf, len);

    // append
    struct outbound_msg** p = &outbound_queue;
    while(*p != NULL) p = &(*p)->next;
    *p = m;
}

static void dequeue_outbound(void)
{
    if(!outbound_queue) err(1, "enqueue_outbound on empty queue");

    struct outbound_msg* m = outbound_queue;
    outbound_queue = m->next;

    free(m->buf); free(m);
}

static ssize_t send_netlink(int fd, const struct outbound_msg* m)
{
    static uint32_t seq;

    size_t msg_len = sizeof(struct nlmsghdr) + m->len;
    if(msg_len > UINT32_MAX || m->len >= UINT32_MAX - sizeof(struct nlmsghdr))
        err(1, "buffer too big to send");

    struct nlmsghdr nhd = {
        .nlmsg_len = msg_len,
        .nlmsg_type = m->type,
        .nlmsg_flags = m->flags,
        .nlmsg_seq = seq++,
        .nlmsg_pid = 0,
    };

    struct iovec vs[] = {
        { .iov_base = &nhd, .iov_len = sizeof(nhd) },
        { .iov_base = m->buf, .iov_len = m->len },
    };

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, 0 };

    struct msghdr mhd = {
        .msg_name = &sa, .msg_namelen = sizeof(sa),
        .msg_iov = vs, .msg_iovlen = LENGTH(vs),
    };

    ssize_t r = sendmsg(fd, &mhd, 0);
    if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        perror("sendto"), exit(1);

    return r;
}

static ssize_t recv_netlink(int fd, void* buf, size_t len)
{
    struct sockaddr_nl sa = { 0 };
    struct nlmsghdr nhd = { 0 };

    struct iovec vs[] = {
        { .iov_base = &nhd, .iov_len = sizeof(nhd) },
        { .iov_base = buf, .iov_len = len },
    };

    struct msghdr mhd = {
        .msg_name = &sa, .msg_namelen = sizeof(sa),
        .msg_iov = vs, .msg_iovlen = LENGTH(vs),
    };

    ssize_t r = recvmsg(fd, &mhd, 0);

    if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        perror("recvmsg"), exit(1);

    if(sa.nl_family != AF_NETLINK || mhd.msg_namelen != sizeof(sa))
        err(1, "wrong kind of message");

    printf("received netlink: type=%"PRIu16" flags=%"PRIu32"\n",
           nhd.nlmsg_type, nhd.nlmsg_flags);

    return r > 0 ? r - sizeof(nhd) : 0;
}

static void teardown_netlink(int fd)
{
    if(close(fd) != 0) perror("close"), exit(1);
}

void run_event_loop(int nfd) {
    struct pollfd fds[] = { { .fd = nfd, .events = POLLIN } };

    size_t timeouts = 0;

    while(1) {
        if(outbound_queue) fds[0].events |= POLLOUT;

        int r = poll(fds, LENGTH(fds), 1000);
        if(r < 0) perror("poll()"), exit(1);

        if(r == 0) {
            printf("still around... (%zu/2)\n", ++timeouts);
            if(timeouts == 2) break;
        }

        if(fds[0].revents & POLLIN) {
            unsigned char buf[4096];
            (void)recv_netlink(fds[0].fd, buf, sizeof(buf));
        }

        if(fds[0].revents & POLLOUT) {
            (void)send_netlink(fds[0].fd, outbound_queue);
            dequeue_outbound();
            if(!outbound_queue) fds[0].events &= ~POLLOUT;
        }
    }
}

int main(void)
{
    printf("hello\n");

    enqueue_outbound(0, NLM_F_ACK, "foo", 3);

    int nfd = setup_netlink(SOCK_NONBLOCK | SOCK_CLOEXEC);
    run_event_loop(nfd);
    teardown_netlink(nfd);

    printf("good bye...\n");
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
