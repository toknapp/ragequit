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
QEMU_OPTS += -enable-kvm -m 256
QEMU_OPTS += -monitor unix:$(MONITOR_SOCK),server,nowait
QEMU_OPTS += -append "root=/dev/ram0 ro console=hvc0"
QEMU_OPTS += -chardev stdio,id=stdio,mux=on,signal=on
QEMU_OPTS += -device virtio-serial-pci -device virtconsole,chardev=stdio
QEMU_OPTS += -mon chardev=stdio -display none

run: $(KERNEL) $(INITRD)
	$(QEMU) $(QEMU_OPTS) -kernel $(KERNEL) -initrd $(INITRD)

kill:
	test -f $(PIDFILE) && kill $(file <$(PIDFILE))

power-off:
	echo "system_power" | socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null

stop:
	echo "quit" | socat - UNIX-CONNECT:$(MONITOR_SOCK) > /dev/null

# initramfs
.PHONY: $(INITRD)
$(INITRD):
	rm -rf $(BUILD)
	mkdir -p $(BUILD)
	$(MAKE) -C src install
	(cd $(BUILD) && find . | cpio --quiet -H newc -o | gzip) > $@

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

.PHONY: run kill power-off stop
.PHONY: clean
