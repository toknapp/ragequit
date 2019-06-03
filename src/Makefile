BUILD ?= ../root

install: init
	install -D -t $(BUILD)/sbin init

init: init.c
	gcc -static -o init init.c

.PHONY: install
