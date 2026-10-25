#!/bin/sh
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /tmp/pari-IDENTIFIANT_NEUF" >&2
    exit 2
fi
case "$1" in
    /*) ;;
    *) echo "chemin absolu requis" >&2; exit 2 ;;
esac
if [ -e "$1" ] || [ -L "$1" ]; then
    echo "refus: chemin déjà utilisé: $1" >&2
    exit 2
fi
mkdir -m 700 "$1"
build_dir=$1
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cc -O2 -std=c11 -Wall -Wextra -Werror -pedantic "$source_dir/pari.c" -lm -o "$build_dir/pari"
"$build_dir/pari" selftest > "$build_dir/selftest.txt"
"$build_dir/pari" run > "$build_dir/resultats.txt"
sha256sum "$source_dir/pari.c" "$source_dir/README.md" "$build_dir/pari" "$build_dir/selftest.txt" "$build_dir/resultats.txt" > "$build_dir/SHA256SUMS"
cat "$build_dir/selftest.txt" "$build_dir/resultats.txt"
