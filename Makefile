# Octet-ADN : C + assembleur GNU → un seul ELF x86_64 statique. Aucun interpréteur.
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation
CFLAGS  += -Iinclude
LDFLAGS ?=
LDLIBS  := -lm

# Toujours un répertoire de build NEUF. Aucun objet partagé avec un ELF vivant.
# Exemple : make experiments/rencontres-build/essai-001/terrarium
RENCONTRES_SRC := experiments/rencontres.c experiments/terrarium.c $(wildcard experiments/rencontres-ligne/*.[ch])
RENCONTRES_CORE := src/rng.c src/isa.c src/vm.c src/mutate.c src/world.c src/labs.c src/store.c src/runner.c src/seeds.S src/labs_h.c src/novelty.c src/soup.c
experiments/rencontres-build/%/terrarium: $(RENCONTRES_SRC) $(RENCONTRES_CORE) include/octet.h
	@test ! -e "$@" && test ! -L "$@" || { echo "Refus : choisir un chemin ELF neuf ($@ existe)"; exit 1; }
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -pthread -o $@ experiments/rencontres.c experiments/rencontres-ligne/echange.c experiments/rencontres-ligne/naissance.c experiments/rencontres-ligne/rendezvous.c $(RENCONTRES_CORE) $(LDLIBS) -ldl

experiments/rencontres-build/%/garde: experiments/rencontres-ligne/garde.c
	@test ! -e "$@" && test ! -L "$@" || { echo "Refus : choisir un chemin ELF neuf ($@ existe)"; exit 1; }
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -o $@ $<

experiments/rencontres-build/%/test-garde: experiments/rencontres-ligne/test_garde.c
	@test ! -e "$@" && test ! -L "$@" || { echo "Refus : choisir un chemin ELF neuf ($@ existe)"; exit 1; }
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -o $@ $<

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

# Recettes + CMAX. Binaire à part : ne pas remplacer terrarium_run (mondes vivants).
experiments/terrarium_cmax: experiments/terrarium.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Stress, masque d'espèces, sociétés intra/inter. Binaire à part : ne pas remplacer les mondes vivants.
experiments/terrarium_soc: experiments/terrarium.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Attention : réponse au choix et au processus qui y mène. Binaire à part.
experiments/terrarium_attn: experiments/terrarium.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Découverte de caractéristiques. Binaire à part : ne pas remplacer les mondes vivants.
experiments/terrarium_traits: experiments/terrarium.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Dépense, causales, saison, tronc. Binaires à part : ne pas remplacer les mondes vivants.
experiments/terrarium_depense experiments/terrarium_causales experiments/terrarium_saison experiments/terrarium_tronc: experiments/terrarium.c $(CORE) include/octet.h experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS) -ldl

experiments/tronc-ligne/exemple.so: experiments/tronc-ligne/exemple.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<

# Lois branchables du tronc. Objets partagés seulement : ne pas lier un terrarium_*.
experiments/tronc-ligne/osmo.so: experiments/tronc-ligne/osmo.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<
experiments/tronc-ligne/competition.so: experiments/tronc-ligne/competition.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<
experiments/tronc-ligne/heritage.so: experiments/tronc-ligne/heritage.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<
experiments/tronc-ligne/memoire.so: experiments/tronc-ligne/memoire.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<
experiments/tronc-ligne/renaissance.so: experiments/tronc-ligne/renaissance.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<

# Lecture de brique. Binaire à part : ne pas remplacer les mondes vivants.
experiments/terrarium_lecture: experiments/terrarium.c $(CORE) include/octet.h experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS) -ldl

experiments/tronc-ligne/lecture.so: experiments/tronc-ligne/lecture.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<
experiments/tronc-ligne/trace.so: experiments/tronc-ligne/trace.c experiments/tronc-ligne/loi.h
	$(CC) $(CFLAGS) -Iexperiments/tronc-ligne -shared -fPIC -o $@ $<

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

experiments/join_wheel: experiments/join_wheel.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/monde_v0: experiments/monde_v0.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/ensembles: experiments/ensembles.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/dispers: experiments/dispers.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

experiments/soupe_spatiale: experiments/soupe_spatiale.c experiments/universe_search.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -pthread -o $@ $< $(CORE) $(LDLIBS)

# Une bande seule : retour après une atteinte, sans partenaire et sans monde.
experiments/seul: experiments/seul.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Le 48/15 du motif seul est-il l'écho d'une période de 16 ?
experiments/motif_artefact: experiments/motif_artefact.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Sonde de phase : le détecteur de réparation, contrôlé (jumeaux, lésion à une phase).
experiments/sonde_phase: experiments/sonde_phase.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Compétition entre les 6 lignées qui ont envahi la soupe 32.
experiments/competition: experiments/competition.c $(CORE) include/octet.h
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDLIBS)

# Lois locales figées, sans ISA : un ensemble partagé naît-il du bruit ?
experiments/univers_innovant: experiments/univers_innovant.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

test: tests/test_vm octet-adn no-python experiments/metabolic_order experiments/mi_detector experiments/terrarium_run experiments/universe_search experiments/universe_search2 experiments/k8_survivor experiments/rebirth experiments/law_keepers experiments/local_laws experiments/incubation_stepback experiments/join_wheel experiments/monde_v0 experiments/ensembles experiments/dispers experiments/soupe_spatiale experiments/seul experiments/motif_artefact experiments/sonde_phase experiments/competition
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
	./experiments/join_wheel selftest
	./experiments/monde_v0 selftest
	./experiments/ensembles selftest
	./experiments/dispers selftest
	./experiments/soupe_spatiale selftest
	./experiments/seul selftest
	./experiments/motif_artefact
	./experiments/sonde_phase selftest
	./experiments/competition selftest

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
	rm -f experiments/general_transfer experiments/general_regs experiments/metabolic_order experiments/gene_transplant experiments/mi_detector experiments/terrarium_run experiments/universe_search experiments/universe_search2 experiments/k8_survivor experiments/rebirth experiments/law_keepers experiments/local_laws experiments/incubation_stepback experiments/join_wheel experiments/monde_v0 experiments/ensembles experiments/dispers experiments/soupe_spatiale experiments/seul experiments/motif_artefact experiments/univers_innovant experiments/sonde_phase experiments/competition

.PHONY: all static test no-python run clean sanitize
