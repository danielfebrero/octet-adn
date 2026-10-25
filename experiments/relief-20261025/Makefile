CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic -Werror
BUILD ?= build-dev

.PHONY: all test sanitize clean
all: $(BUILD)/relief
$(BUILD)/relief: relief.c
	@test ! -e '$@' || { echo 'ELF existant : choisir BUILD neuf'; exit 1; }
	@mkdir -p '$(BUILD)'
	$(CC) $(CFLAGS) -o '$@' relief.c -lm

test: all
	'$(BUILD)/relief' selftest
	sh tests.sh '$(BUILD)/relief'
sanitize:
	$(MAKE) BUILD=build-san CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	@echo 'Aucun nettoyage automatique : les ELF sont conservés; choisir un BUILD neuf.'
