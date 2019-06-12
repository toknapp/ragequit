QEMU ?= qemu-system-x86_64

# dirs and files
export BUILD = $(shell pwd)/root
KERNEL_ROOT = linux
KERNEL_CONFIG = config
PIDFILE = .pid
MONITOR_SOCK = .monitor.sock
KERNEL = bzImage
INITRD = initrd.cpio.gz

# qemu
QEMU_OPTS = -pidfile $(PIDFILE) -no-reboot
QEMU_OPTS += -m 256
QEMU_OPTS += -monitor unix:$(MONITOR_SOCK),server,nowait
QEMU_OPTS += -append "root=/dev/ram0 ro console=hvc0"
QEMU_OPTS += -chardev stdio,id=stdio,signal=on
QEMU_OPTS += -device virtio-serial-pci
QEMU_OPTS += -device virtconsole,chardev=stdio
QEMU_OPTS += -display none

TIMEOUT ?= 10s
test: $(KERNEL) $(INITRD)
	script /dev/null --quiet \
		--command='timeout $(TIMEOUT) $(QEMU) $(QEMU_OPTS) -kernel $(KERNEL) -initrd $(INITRD)'

build: $(KERNEL) $(INITRD)

kill:
	test -f $(PIDFILE) && kill $(file <$(PIDFILE))

power-off:
	[ -S $(MONITOR_SOCK) ] \
		&& (echo "system_powerdown" | socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null) \
		&& echo 1>&2 "power-off" \
		|| echo 1>&2 "no socket: $(MONITOR_SOCK)"

power-off-loop:
	while sleep 1; do make --no-print-directory --silent power-off; done

stop:
	echo "quit" | socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null

# initramfs
.PHONY: $(INITRD)
$(INITRD):
	rm -rf "$(BUILD)"
	mkdir -p "$(BUILD)"
	$(MAKE) -C src install
	(cd "$(BUILD)" && find . | cpio --quiet -H newc -o | gzip) > $@

# kernel
$(KERNEL): config $(KERNEL_ROOT)
	install -m 644 --backup=numbered config $(KERNEL_ROOT)/.config
	$(MAKE) -C $(KERNEL_ROOT)
	cp $(KERNEL_ROOT)/arch/x86/boot/bzImage $(KERNEL)

menuconfig: $(KERNEL_ROOT)
	install -m 644 --backup=numbered config $(KERNEL_ROOT)/.config
	$(MAKE) -C $(KERNEL_ROOT) menuconfig
	install -m 644 --backup=numbered $(KERNEL_ROOT)/.config config

$(KERNEL_ROOT):
	mkdir -p $@
	curl --silent https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.1.7.tar.xz \
		| tar -Jx -C $@ --strip-components=1

# auxiliary
clean:
	rm -rf root
	$(MAKE) -C src clean

.PHONY: run kill power-off stop
.PHONY: clean
