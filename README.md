# ragequit

A bare metal tool to convert [ACPI](https://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface)
events into signals, without using [acpid](https://wiki.archlinux.org/index.php/Acpid)
or any other high-level dependencies:
```
Power off button -> ACPI -> netlink -> ragequit -> kill(pid, SIGINT)
```
