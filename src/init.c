#include <stdio.h>

#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

int main(void)
{
    printf("hello\n");
    for(size_t i = 0; i < 5; i++) {
        printf("still around... (%zu/5)\n", i);
        sleep(1);
    }
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
