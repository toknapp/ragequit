#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

static ssize_t send_netlink(int fd, uint16_t type, uint32_t flags,
                            const void* buf, size_t len)
{
    static uint32_t seq = 0;

    size_t msg_len = sizeof(struct nlmsghdr) + len;
    if(msg_len > UINT32_MAX || len >= UINT32_MAX - sizeof(struct nlmsghdr))
        err(1, "buffer too big to send");

    struct nlmsghdr nhd = {
        .nlmsg_len = msg_len,
        .nlmsg_type = type,
        .nlmsg_flags = flags,
        .nlmsg_seq = seq++,
        .nlmsg_pid = 0,
    };

    struct iovec vs[] = {
        { .iov_base = &nhd, .iov_len = sizeof(nhd) },
        { .iov_base = (void*)buf, .iov_len = len },
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

    if(r > 0) return r - sizeof(nhd);
    else return 0;
}

static void teardown_netlink(int fd)
{
    if(close(fd) != 0) perror("close"), exit(1);
}

void run_event_loop(int fd) {
    struct pollfd fds[] = { { .fd = fd, .events = POLLIN } };

    size_t timeouts = 0;

    while(1) {
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
    }
}

int main(void)
{
    printf("hello\n");

    int nfd = setup_netlink(SOCK_NONBLOCK | SOCK_CLOEXEC);

    (void) send_netlink(nfd, 0, NLM_F_ACK, "foo", 3);

    run_event_loop(nfd);

    teardown_netlink(nfd);

    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
