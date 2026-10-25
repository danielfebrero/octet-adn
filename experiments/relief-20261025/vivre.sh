#!/bin/sh
# Bounded runs checkpoint and close journals before lossless segment rotation.
set -eu
dir=$1
elf=$2
child=
finish() {
  trap '' TERM INT
  if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
  exit 0
}
trap finish TERM INT
mkdir -p "$dir/histoire"
while :; do
  "$elf" inspect "$dir" > "$dir/dernier-etat.txt"
  if grep -q '^EXTINCTION:' "$dir/dernier-etat.txt"; then
    cat "$dir/dernier-etat.txt"
    exit 0
  fi
  "$elf" run "$dir" --ticks 600 --tick-ms 100 &
  child=$!
  wait "$child"
  child=
  # No writer exists here: rename entire segment, retain header, gzip.
  for name in events series; do
    file="$dir/$name.tsv"
    if [ "$(stat -c %s "$file")" -ge 33554432 ]; then
      header=$(head -n 1 "$file")
      stamp=$(date -u +%Y%m%dT%H%M%S)
      archived="$dir/histoire/$name-$stamp.tsv"
      test ! -e "$archived"
      mv "$file" "$archived"
      printf '%s\n' "$header" > "$file"
      gzip "$archived"
    fi
  done
  # Historical segments are never deleted automatically.
done
