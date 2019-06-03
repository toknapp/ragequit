#include <stdio.h>
#include <stdlib.h>

#include <asm/types.h>
#include <linux/reboot.h>
#include <linux/netlink.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <unistd.h>

int setup_netlink(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
    if(fd < 0) perror("socket()"), abort();

    struct sockaddr_nl sa = { 0 };
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = 0;

    int r = bind(fd, (struct sockaddr*)&sa, sizeof(sa));
    if(r != 0) perror("bind()"), abort();

    return fd;
}

void teardown_netlink(int fd)
{
    if(close(fd) != 0) perror("close"), abort();
}

int main(void)
{
    printf("hello\n");

    int nfd = setup_netlink();

    for(size_t i = 0; i < 2; i++) {
        printf("still around... (%zu/2)\n", i);
        sleep(1);
    }

    teardown_netlink(nfd);

    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
