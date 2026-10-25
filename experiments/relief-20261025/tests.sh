#!/bin/sh
set -eu
exe=$(readlink -f "$1")
root=$(mktemp -d /tmp/relief-cli-XXXXXX)
child=
trap 'if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi; rm -rf "$root"' EXIT HUP INT TERM
expect_failure() { if "$@" >"$root/rejection.log" 2>&1; then echo "Unexpected acceptance: $*" >&2; exit 1; fi; }
expect_failure "$exe" init "$root/invalid" --width 0
expect_failure "$exe" init "$root/overflow" --capacity 4294967296
"$exe" init "$root/direct" --width 40 --height 32 --capacity 512 --founders 100 --seed 73 --season 64 --sample 20 --checkpoint 40 >/dev/null
expect_failure "$exe" init "$root/direct"
cp -a "$root/direct" "$root/split"
"$exe" run "$root/direct" --ticks 200 >/dev/null
"$exe" run "$root/split" --ticks 73 >/dev/null
# Mimic torn/uncommitted journal tail; must disappear on recovery.
printf '999999\tghost\n' >> "$root/split/events.tsv"
printf 'incomplete' >> "$root/split/series.tsv"
"$exe" run "$root/split" --ticks 127 >/dev/null
cmp "$root/direct/state.ck" "$root/split/state.ck"
cmp "$root/direct/events.tsv" "$root/split/events.tsv"
! rg -q 'ghost|incomplete' "$root/split/events.tsv" "$root/split/series.tsv"
cp -a "$root/direct" "$root/corrupt"
# Flip checksum byte deterministically: every bit changes.
old=$(od -An -tu1 -j40 -N1 "$root/corrupt/state.ck")
new=$((old ^ 255))
escaped=$(printf '%03o' "$new")
printf "\\$escaped" | dd of="$root/corrupt/state.ck" bs=1 seek=40 conv=notrunc status=none
expect_failure "$exe" inspect "$root/corrupt"
cp "$root/direct/state.ck" "$root/corrupt/state.ck"
printf x >> "$root/corrupt/state.ck"
expect_failure "$exe" inspect "$root/corrupt"
# SIGTERM delivered during a real run: checkpoint and journals must be usable.
"$exe" init "$root/signal" --width 256 --height 128 --capacity 8192 --founders 2048 --sample 10 --checkpoint 100 >/dev/null
"$exe" run "$root/signal" >"$root/signal.out" &
child=$!
sleep 0.1
expect_failure "$exe" run "$root/signal" --ticks 1
kill -TERM "$child"
wait "$child"
child=
"$exe" inspect "$root/signal" >/dev/null
"$exe" run "$root/signal" --ticks 1 >/dev/null
# Zero founders is a deliberate sterile/extinction test, not a restart policy.
"$exe" init "$root/empty" --width 8 --height 8 --capacity 4 --founders 0 >/dev/null
cp "$root/empty/state.ck" "$root/empty-before.ck"
"$exe" run "$root/empty" --ticks 100 >/dev/null
cmp "$root/empty-before.ck" "$root/empty/state.ck"
printf '%s\n' 'CLI PASS: exact checkpoint+event replay; invalid parameters/corruption rejected; exclusive writer; SIGTERM recovery; zero reseeding'
