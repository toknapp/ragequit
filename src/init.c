#define RAGEQUIT_IMPLEMENTATION
#include "ragequit.h"

#include <linux/reboot.h>
#include <sys/reboot.h>
#include <stdio.h>

static void power_off(void* opaque)
{
    struct ragequit_state* st = opaque;
    ragequit_deinitialize(st);

    printf("good bye...\n");

    (void)reboot(LINUX_REBOOT_CMD_POWER_OFF);
    perror("reboot(.._POWER_OFF)"), exit(1);
}

int main(void)
{
    printf("hello\n");

    struct ragequit_state st;
    ragequit_initialize(&st, /* non_blocking */ 1, power_off, &st);
    ragequit_run_event_loop(&st);
}
