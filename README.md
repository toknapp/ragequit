# ragequit

[![CircleCI](https://circleci.com/gh/toknapp/ragequit.svg?style=svg)](https://circleci.com/gh/toknapp/ragequit)

A bare metal [header-only library](https://github.com/nothings/stb)
to convert [ACPI](https://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface)
events into signals, without using [acpid](https://wiki.archlinux.org/index.php/Acpid)
or any other dependencies (e.g. [libnetlink](https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/tree/lib/libnetlink.c)
or [libmnl](https://netfilter.org/projects/libmnl/)):
```
Power off button -> ACPI -> netlink -> ragequit -> kill(pid, SIGINT)
```
