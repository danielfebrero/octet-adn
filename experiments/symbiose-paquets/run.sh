#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(mktemp -d /tmp/octet-symbiose-0996a2cd.XXXXXXXX)
cd "$root"
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-format-truncation -Iinclude \
  experiments/symbiose-paquets/symbiose.c \
  src/rng.c src/isa.c src/vm.c src/mutate.c src/world.c src/labs.c \
  src/store.c src/runner.c src/seeds.S src/labs_h.c src/novelty.c src/soup.c \
  -lm -o "$build/symbiose"
printf 'ELF neuf : %s\n' "$build/symbiose" >&2
"$build/symbiose" selftest
"$build/symbiose" run
