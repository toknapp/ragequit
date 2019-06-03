#include <stdio.h>

#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

void main(void)
{
    printf("hello world!\n");
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
