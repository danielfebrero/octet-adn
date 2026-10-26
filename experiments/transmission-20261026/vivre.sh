#!/bin/sh
set -eu
world=${1:?world}; elf=${2:?elf}
child=
stop() { if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi; exit 0; }
trap stop TERM INT HUP
# Resume only. There is intentionally no init path in this supervisor.
test -s "$world/state.ck"
while :; do
  "$elf" inspect "$world"
  if "$elf" inspect "$world" | head -n 1 | grep -q 'population=0 '; then exit 0; fi
  "$elf" run "$world" 600 100 &
  child=$!
  wait "$child"
  child=
done
