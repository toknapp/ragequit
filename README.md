# ragequit

[![CircleCI](https://circleci.com/gh/toknapp/ragequit.svg?style=svg&circle-token=c533511f1502da25765938914ae3d8b8e4e55a72)](https://circleci.com/gh/toknapp/ragequit)

A bare metal [header-only library](https://github.com/nothings/stb)
to convert [ACPI](https://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface)
events into signals, without using [acpid](https://wiki.archlinux.org/index.php/Acpid)
or any other dependencies (e.g. [libnetlink](https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/tree/lib/libnetlink.c)
or [libmnl](https://netfilter.org/projects/libmnl/)):
```
Power off button -> ACPI -> netlink -> ragequit -> callback
```

## Currently supported device classes
* `button/power`

Add support for your Thinkpad [here](https://github.com/toknapp/ragequit/blob/36ee56a5bf99eec6c6aaf85f5a16da728b8836dd/src/ragequit.h#L328).

## Example
The following snippet is taken from the test's [`init`](src/init.c) process:
```c
#define RAGEQUIT_IMPLEMENTATION
#include "ragequit.h"

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
    ragequit_initialize(&st, power_off, &st);
    ragequit_run_event_loop(&st);
}
```
