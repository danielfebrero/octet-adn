# Octet-ADN : C + assembleur GNU → un seul ELF x86_64 statique. Aucun interpréteur.
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation
CFLAGS  += -Iinclude
LDFLAGS ?=
LDLIBS  := -lm

CORE := src/rng.o src/isa.o src/vm.o src/mutate.o src/world.o src/labs.o src/store.o src/runner.o src/seeds.o
SEEDS := $(wildcard seeds/*.adn)

all: octet-adn

octet-adn: $(CORE) src/main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	strip $@

src/seeds.o: src/seeds.S $(SEEDS)
	$(CC) -c -o $@ $<

src/%.o: src/%.c include/octet.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_vm: tests/test_vm.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ tests/test_vm.c $(CORE) $(LDLIBS)

test: tests/test_vm octet-adn no-python
	./tests/test_vm

# garde-fou « octet only » : aucun source interprété versionné
no-python:
	@bad=$$(git ls-files 2>/dev/null | grep -E '\.(py|pyc|js|ts|rb|pl|lua|sh|ipynb|toml|ya?ml|json|jsonl)$$' || true); \
	if [ -n "$$bad" ]; then echo "fichiers interdits :"; echo "$$bad"; exit 1; fi; \
	echo "no-python : OK (aucun .py/.js/.rb/.sh/.json/.yaml versionné)"

# binaire autonome (glibc statique, ~1 Mo), pour copier sur une autre machine Linux x86_64
static: $(CORE) src/main.o
	$(CC) $(CFLAGS) -static -o octet-adn-static $^ $(LDLIBS)
	strip octet-adn-static

run: octet-adn
	./octet-adn --labs all --ticks-per-cycle 200

clean:
	rm -f src/*.o octet-adn octet-adn-static tests/test_vm

.PHONY: all static test no-python run clean
