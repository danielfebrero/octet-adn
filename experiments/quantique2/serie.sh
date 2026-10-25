#!/bin/sh
# Série préenregistrée Q2 (PROTOCOLE.md) : 20 graines × 4 bras, mondes neufs sous RACINE.
set -eu
elf=$1; root=$2; par=${3:-8}
[ -x "$elf" ] || { echo "ELF absent : $elf" >&2; exit 1; }
[ ! -e "$root" ] || { echo "RACINE existe déjà : $root" >&2; exit 1; }
mkdir -p "$root"
for s in $(seq 1001 1020); do for a in E H S U; do echo "$s $a"; done; done | xargs -P "$par" -n 2 sh -c '
  case "$3" in E) x="--imprint 1 --cool 100";; H) x="--imprint 2 --cool 100";; S) x="--imprint 0 --cool 100";; U) x="--imprint 1 --cool 0";; esac
  nice -n 19 "$0" run "$1/$3-$2" --ticks 16384 --seed "$2" $x --n 96 --box 12 --grid 6 --modes 64 --period 16 \
    --temp 0.6 --gamma 0.5 --rep 1.5 --xi 2 --rcut 5 --spec 0.5 --gain 1 --bind-r 1.4 --window 1024 \
    > "$1/$3-$2.out" 2>&1 || echo "échec $3 $2" >&2' "$elf" "$root"
