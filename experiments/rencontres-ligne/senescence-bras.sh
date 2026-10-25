#!/bin/sh
# Série préenregistrée SENESCENCE-BRAS.md : 20 graines × 3 bras, mondes neufs sous RACINE.
# N'ouvre aucun monde existant (rencontres refuse un monde protégé ; RACINE doit être neuve).
set -eu
elf=$1; root=$2; par=${3:-8}
[ -x "$elf" ] || { echo "ELF absent : $elf" >&2; exit 1; }
[ ! -e "$root" ] || { echo "RACINE existe déjà : $root" >&2; exit 1; }
mkdir -p "$root"
for s in $(seq 1001 1020); do for p in senescence senescence-decouple senescence-inverse; do
  echo "$s $p"; done; done | xargs -P "$par" -n 2 sh -c '
  nice -n 19 "$0" run --dir "$1/$3-$2" --profile "$3" --ticks 100000 --islands 1 --w 32 --h 16 \
    --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --seed "$2" > "$1/$3-$2.log" 2>&1 \
    || echo "échec $3 $2" >&2' "$elf" "$root"
