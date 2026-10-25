#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d /tmp/octet-0996a2cd-heritage-build.XXXXXXXX)
proof_dir=$(mktemp -d /tmp/octet-0996a2cd-heritage-preuves.XXXXXXXX)
cc -O2 -std=gnu11 -Wall -Wextra -Werror -Iinclude \
    experiments/heritage-milieu/heritage.c \
    src/world.c src/vm.c src/mutate.c src/rng.c src/isa.c \
    -lm -o "$build_dir/heritage"
"$build_dir/heritage" selftest > "$proof_dir/selftest.txt"
"$build_dir/heritage" run > "$proof_dir/comptes.txt"
sha256sum "$build_dir/heritage" "$proof_dir/selftest.txt" "$proof_dir/comptes.txt" > "$proof_dir/sha256.txt"
cat "$proof_dir/selftest.txt"
tail -n 2 "$proof_dir/comptes.txt"
printf 'Binaire : %s/heritage\nPreuves : %s\n' "$build_dir" "$proof_dir"
