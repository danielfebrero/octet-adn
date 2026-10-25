#!/bin/sh
# Série préenregistrée (PROTOCOLE.md). À lancer depuis experiments/ du dépôt hôte (mondes en chemins relatifs,
# lus en lecture seule). Chaque ligne = les arguments complets d'un appel à transplant.
set -eu
elf=$1; root=$2; par=${3:-8}
[ -x "$elf" ] || { echo "ELF absent : $elf" >&2; exit 1; }
[ ! -e "$root" ] || { echo "RACINE existe déjà : $root" >&2; exit 1; }
mkdir -p "$root"
jobs="$root/travaux.txt"
for s in $(seq 1001 1010); do
  for p in "vie-monde vie-forces" "vie-monde vie-causes" "vie-forces vie-causes"; do
    set -- $p
    echo "jardin $1 $1 $2 $root/A-$1-$2-hote-$1-$s 20000 $s 64"
    echo "jardin $2 $1 $2 $root/A-$1-$2-hote-$2-$s 20000 $s 64"
  done
  for x in vie-monde vie-forces vie-causes; do echo "jardin $x $x $x $root/A-$x-$x-hote-$x-$s 20000 $s 64"; done
  for p in "vie-capture-3a1923e vie-senescence-3a1923e" "vie-capture-3a1923e vie-parente-3a1923e" \
           "vie-capture-3a1923e vie-rencontres-claude-3a1923e" "vie-senescence-3a1923e vie-parente-3a1923e" \
           "vie-senescence-3a1923e vie-rencontres-claude-3a1923e" "vie-parente-3a1923e vie-rencontres-claude-3a1923e"; do
    set -- $p; echo "jardin vie-senescence-3a1923e $1 $2 $root/B-$1-$2-$s 20000 $s 32"
  done
  for src in vie-monde vie-causes; do
    other=vie-causes; [ "$src" = vie-causes ] && other=vie-monde
    echo "teleport $src - $root/C-$src-temoin-$s temoin 0 10000 $s"
    echo "teleport $src - $root/C-$src-retrait-$s retrait 0.5 10000 $s"
    echo "teleport $src $other $root/C-$src-etranger-$s arrivee 0.1 10000 $s"
    echo "teleport $src $src $root/C-$src-meme-$s arrivee 0.1 10000 $s"
  done
done > "$jobs"
xargs -P "$par" -L 1 sh -c 'ulimit -c 0; nice -n 19 "$0" "$@" 2>&1 || echo "échec : $*"' "$elf" < "$jobs" > "$root/journal.txt" 2>&1
