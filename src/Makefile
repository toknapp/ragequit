BUILD ?= ../root
GCC ?= gcc

CFLAGS := -Werror -Wall -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2
EXTRA_CFLAGS ?= -g0 -O2
LDFLAGS := -static

install: init
	install -D -t $(BUILD)/sbin init

init: init.c
	$(GCC) $(LDFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $^

clean:
	rm -f init

.PHONY: install clean
