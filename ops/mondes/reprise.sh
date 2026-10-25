#!/bin/sh
# Inventaire relu à chaque passage ; aucune interruption des mondes vivants.
set -u
# ARCHIVED-20f62d86: all34 retired by direct operator order.
echo "Mondes archivés le 25/10/2026 : reprise automatique désactivée." >&2
exit 0
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
manifest=${OCTET_MANIFEST:-$script_dir/mondes.tsv}
cd "${OCTET_REPO_DIR:-/data/eternalgrok-grok-workspace/produits/octet-adn}" || exit 1
alive() {
  for f in /proc/[0-9]*/cmdline; do
    cmd=$(tr '\0' ' ' < "$f" 2>/dev/null || true)
    case "$cmd" in *"run --dir $1 --ticks 0"*) return 0 ;; esac
  done
  return 1
}
launch() {
  bin=$1; dir=$2
  if alive "$dir"; then echo "vivant $dir"; return; fi
  if [ ! -x "$bin" ] || [ ! -s "$dir/state.ck" ]; then
    echo "reprise refusée : ELF ou checkpoint absent $dir" >&2; return
  fi
  printf '%s reprise service binaire=%s sans choc\n' "$(date -u +%FT%TZ)" "$bin" >> "$dir/boot.log"
  "$bin" run --dir "$dir" --ticks 0 >> "$dir/boot.log" 2>&1 9>&- &
  echo "relance $dir pid $!"
}
check() {
  test -r "$manifest" || return 1
  awk 'NF && $1 !~ /^#/ {if (NF != 2 || seen[$2]++) exit 1; n++} END {if (!n) exit 1}' "$manifest" || return 1
  while read -r bin dir; do
    case "$bin" in ''|\#*) continue ;; esac
    case "$bin:$dir" in *..*|/*|*:/*) return 1 ;; esac
    test -x "$bin" && test -s "$dir/state.ck" || return 1
  done < "$manifest"
}
if [ "${1:-}" = --check ]; then check; exit $?; fi
# Une instance unique, même pendant un remplacement du superviseur.
exec 9> .octet-mondes-superviseur.lock
flock 9
while :; do
  if check; then
    while read -r bin dir; do
      case "$bin" in ''|\#*) continue ;; esac
      launch "$bin" "$dir"
    done < "$manifest"
  else echo "inventaire invalide : aucune relance" >&2; fi
  sleep 20 &
  wait $!
done
