#!/bin/sh
# Série préenregistrée (PROTOCOLE.md) : 10 milieux × 2 bras × 10 graines, répertoires neufs sous RACINE.
# À lancer depuis la racine du dépôt (graines seeds/).
set -eu
elf=$1; root=$2; par=${3:-10}
[ -x "$elf" ] || { echo "ELF absent : $elf" >&2; exit 1; }
[ ! -e "$root" ] || { echo "RACINE existe déjà : $root" >&2; exit 1; }
mkdir -p "$root"
for t in memoire composition inversion observation patience chainage transfert signal autrui relation; do
  for b in info temoin; do for s in $(seq 1001 1010); do echo "$t $b $s"; done; done
done | xargs -P "$par" -n 3 sh -c '
  nice -n 19 "$0" run "$1/$2-$3-$4" --tache "$2" --bras "$3" --graine "$4" --cycles 65536 > "$1/$2-$3-$4.out" 2>&1 \
    || echo "échec $2 $3 $4" >&2' "$elf" "$root"
