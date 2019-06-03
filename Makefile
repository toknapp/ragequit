QEMU ?= qemu-system-x86_64

# dirs and files
export BUILD = $(shell pwd)/root
KERNEL_ROOT = linux
KERNEL_CONFIG = config
PIDFILE = .pid
MONITOR_SOCK = .monitor.sock
KERNEL = bzImage
INITRD = initrd.cpio

# qemu
QEMU_OPTS = -pidfile $(PIDFILE) -no-reboot
QEMU_OPTS += -enable-kvm -m 256
QEMU_OPTS += -monitor unix:$(MONITOR_SOCK),server,nowait
QEMU_OPTS += -append "root=/dev/ram0 ro console=hvc0"
QEMU_OPTS += -chardev stdio,id=stdio,mux=on,signal=on
QEMU_OPTS += -device virtio-serial-pci -device virtconsole,chardev=stdio
QEMU_OPTS += -mon chardev=stdio -display none

run: $(KERNEL) $(INITRD)
	timeout 4s $(QEMU) $(QEMU_OPTS) -kernel $(KERNEL) -initrd $(INITRD)

kill:
	test -f $(PIDFILE) && kill $(file <$(PIDFILE))

power-off:
	[ -S $(MONITOR_SOCK) ] \
		&& socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null <<< "system_power" \
		&& echo 1>&2 "power-off" \
		|| echo 1>&2 "no socket: $(MONITOR_SOCK)"

power-off-loop:
	while sleep 1; do make --no-print-directory --silent power-off; done

stop:
	echo "quit" | socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null

# initramfs
.PHONY: $(INITRD)
$(INITRD):
	rm -rf $(BUILD)
	mkdir -p $(BUILD)
	$(MAKE) -C src install
	(cd $(BUILD) && find . | cpio --quiet -H newc -o) > $@

# kernel
$(KERNEL): config
	install -m 644 --backup=numbered config $(KERNEL_ROOT)/.config
	$(MAKE) -C $(KERNEL_ROOT)
	cp $(KERNEL_ROOT)/arch/x86/boot/bzImage $(KERNEL)

menuconfig: $(KERNEL_ROOT)
	install -m 644 --backup=numbered config $(KERNEL_ROOT)/.config
	$(MAKE) -C $(KERNEL_ROOT) menuconfig
	install -m 644 --backup=numbered $(KERNEL_ROOT)/.config config

$(KERNEL_ROOT):
	git clone --depth=1 https://github.com/torvalds/linux $@

# auxiliary
clean:
	rm -rf root
	$(MAKE) -C src clean

.PHONY: run kill power-off stop
.PHONY: clean
