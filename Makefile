# Octet-ADN : C + assembleur GNU → un seul ELF x86_64 statique. Aucun interpréteur.
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation
CFLAGS  += -Iinclude
LDFLAGS ?=
LDLIBS  := -lm

CORE := src/rng.o src/isa.o src/vm.o src/mutate.o src/world.o src/labs.o src/store.o src/runner.o src/seeds.o src/labs_h.o
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

# ASan + UBSan : tests + 3 cycles des 10 labos (dossier jetable), arrêt à la première erreur
SAN := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
sanitize:
	@rm -rf /tmp/oadn-san-data
	$(CC) -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Iinclude $(SAN) -o tests/test_vm_san tests/test_vm.c $(filter-out src/seeds.c,$(CORE:.o=.c)) src/seeds.S $(LDLIBS)
	$(CC) -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Iinclude $(SAN) -o octet-adn-san $(filter-out src/seeds.c,$(CORE:.o=.c)) src/main.c src/seeds.S $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=1 ./tests/test_vm_san
	ASAN_OPTIONS=detect_leaks=1 ./octet-adn-san --cycles 3 --no-reload --data-dir /tmp/oadn-san-data > /dev/null
	ASAN_OPTIONS=detect_leaks=1 ./octet-adn-san eval /tmp/oadn-san-data 2 > /dev/null
	@rm -rf /tmp/oadn-san-data
	@echo "sanitize : OK (ASan + UBSan, 0 erreur)"

# binaire autonome (glibc statique, ~1 Mo), pour copier sur une autre machine Linux x86_64
static: $(CORE) src/main.o
	$(CC) $(CFLAGS) -static -o octet-adn-static $^ $(LDLIBS)
	strip octet-adn-static

run: octet-adn
	./octet-adn --labs all --ticks-per-cycle 200

clean:
	rm -f src/*.o octet-adn octet-adn-static octet-adn-san tests/test_vm tests/test_vm_san

.PHONY: all static test no-python run clean sanitize
