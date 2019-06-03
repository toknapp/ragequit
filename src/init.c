#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <asm/types.h>
#include <linux/reboot.h>
#include <linux/netlink.h>
#include <sys/reboot.h>
#include <sys/socket.h>

#define LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))

static int setup_netlink(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
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
    if(msg_len > UINT32_MAX || len >= sizeof(struct nlmsghdr))
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

static void teardown_netlink(int fd)
{
    if(close(fd) != 0) perror("close"), exit(1);
}

int main(void)
{
    printf("hello\n");

    int nfd = setup_netlink();

    (void) send_netlink(nfd, 0, 0, "foo", 3);

    for(size_t i = 0; i < 2; i++) {
        printf("still around... (%zu/2)\n", i);
        sleep(1);
    }

    teardown_netlink(nfd);

    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
