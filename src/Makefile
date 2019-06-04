BUILD ?= ../root
GCC ?= gcc

CFLAGS := -Werror -Wall -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2
EXTRA_CFLAGS ?= -g0 -O2
LDFLAGS := -static

init: init.c
	$(GCC) $(LDFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $^

install: init
	install -D -t $(BUILD)/sbin init

clean:
	rm -f init

.PHONY: install clean
