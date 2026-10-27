CC ?= cc
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-format-truncation
CORE = frozen/core/rng.c frozen/core/isa.c frozen/core/vm.c frozen/core/mutate.c frozen/core/world.c
all: build/legacy build/relief build/transmission build/legacy-terra
build/legacy: legacy.c common.h $(CORE) frozen/core/octet.h
	mkdir -p build
	$(CC) $(CFLAGS) -Ifrozen/core -o $@ legacy.c $(CORE) -lm
build/relief: relief.c common.h frozen/relief.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ relief.c
build/transmission: transmission.c common.h frozen/transmission.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ transmission.c
build/legacy-terra: legacy-terra.c common.h frozen/terrarium.c $(wildcard frozen/core/*.[chS])
	mkdir -p build
	$(CC) $(CFLAGS) -Ifrozen/core -pthread -o $@ legacy-terra.c $(wildcard frozen/core/*.c) frozen/core/seeds.S -lm -ldl

PLUGINS = osmo competition heritage memoire renaissance lecture trace
plugins: $(addprefix build/plugins/,$(addsuffix .so,$(PLUGINS)))
build/plugins/%.so: frozen/plugins/%.c frozen/plugins/loi.h
	mkdir -p build/plugins
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $<
all: plugins
