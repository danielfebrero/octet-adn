# Octet-ADN : C + assembleur GNU → un seul ELF x86_64 statique. Aucun interpréteur.
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation
CFLAGS  += -Iinclude
LDFLAGS ?=
LDLIBS  := -lm

CORE := src/rng.o src/isa.o src/vm.o src/mutate.o src/world.o src/labs.o src/store.o src/runner.o src/seeds.o src/labs_h.o src/novelty.o src/soup.o
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

# Mesure reproductible du transfert minimal entre les lignées general (hors runner).
experiments/general_transfer: experiments/general_transfer.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Levier regs (branche feat/emergence-regs) : chemins à la main et mutants d'un octet.
experiments/general_regs: experiments/general_regs.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Paramètre d'ordre métabolique (branche feat/metabolic-order), aveugle au code.
experiments/metabolic_order: experiments/metabolic_order.c experiments/lifeprobe.h $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Gene-calling par matrice de transplantation (branche feat/gene-transplant-matrix).
experiments/gene_transplant: experiments/gene_transplant.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Terrarium : monde ouvert peuplé, sans fitness (archipel d'îles, un thread par île).
experiments/terrarium_run: experiments/terrarium.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Détecteur de vie par information mutuelle (branche feat/mi-detector), aveugle au code.
experiments/mi_detector: experiments/mi_detector.c experiments/lifeprobe.h $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Méta-évolution d'univers : sélection de physiques (table octet→primitive élémentaire) contre témoin aléatoire.
experiments/universe_search: experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# v2 : fitness robuste (racing), origine à grand N, dérive lente de la physique (inclut universe_search.c sans le modifier).
experiments/universe_search2: experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Autopsie du survivant de la dérive K=8 (inclut universe_search2.c sans son main).
experiments/k8_survivor: experiments/k8_survivor.c experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/rebirth: experiments/rebirth.c experiments/k8_survivor.c experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/law_keepers: experiments/law_keepers.c experiments/rebirth.c experiments/k8_survivor.c experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/local_laws: experiments/local_laws.c experiments/law_keepers.c experiments/rebirth.c experiments/k8_survivor.c experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/incubation_stepback: experiments/incubation_stepback.c experiments/universe_search2.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

test: tests/test_vm octet-adn no-python experiments/metabolic_order experiments/mi_detector experiments/terrarium_run experiments/universe_search experiments/universe_search2 experiments/k8_survivor experiments/rebirth experiments/law_keepers experiments/local_laws experiments/incubation_stepback
	./tests/test_vm
	./experiments/metabolic_order selftest
	./experiments/mi_detector selftest
	./experiments/terrarium_run selftest
	./experiments/universe_search selftest
	./experiments/universe_search2 selftest
	./experiments/k8_survivor selftest
	./experiments/rebirth selftest
	./experiments/law_keepers selftest
	./experiments/local_laws selftest
	./experiments/incubation_stepback selftest

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
	rm -f experiments/general_transfer experiments/general_regs experiments/metabolic_order experiments/gene_transplant experiments/mi_detector experiments/terrarium_run experiments/universe_search experiments/universe_search2 experiments/k8_survivor experiments/rebirth experiments/law_keepers experiments/local_laws experiments/incubation_stepback

.PHONY: all static test no-python run clean sanitize
