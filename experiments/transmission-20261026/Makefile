CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic -Werror
BUILD ?= build-dev
.PHONY: all test
all: $(BUILD)/transmission
$(BUILD)/transmission: transmission.c
	@test ! -e '$@' || { echo 'ELF existant: choisir BUILD neuf'; exit 1; }
	@mkdir -p '$(BUILD)'
	$(CC) $(CFLAGS) -o '$@' transmission.c
test: all
	'$(BUILD)/transmission' selftest
	sh tests.sh '$(BUILD)/transmission'
