#!/bin/sh
set -eu
elf=$(readlink -f "$1")
source_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
root=$(mktemp -d /tmp/transmission-tests-XXXXXX)
child=
trap 'if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi; rm -rf "$root"' EXIT HUP INT TERM
reject() { if "$@" >"$root/rejected" 2>&1; then echo "Unexpected acceptance: $*" >&2; exit 1; fi; }
"$elf" init "$root/direct" "$source_dir/initial-fr.txt" 32 73 > /dev/null
reject "$elf" init "$root/direct" "$source_dir/initial-fr.txt"
cp -a "$root/direct" "$root/split"
"$elf" run "$root/direct" 7000 0 > "$root/direct.out"
"$elf" run "$root/split" 2300 0 > /dev/null
printf ghost > "$root/split/mail/9999999999999999.bin"
"$elf" run "$root/split" 4700 0 > /dev/null
cmp "$root/direct/state.ck" "$root/split/state.ck"
test ! -e "$root/split/mail/9999999999999999.bin"
diff -r "$root/direct/mail" "$root/split/mail"
"$elf" inspect "$root/direct"
test "$(find "$root/direct/mail" -name '*.bin' | wc -l)" -gt 0
python3 "$source_dir/collect.py" "$root/direct" > "$root/pickup.json"
python3 - "$root/pickup.json" <<'PY'
import json,sys
m=json.load(open(sys.argv[1]));assert m['messages'];assert all(int(x['metadata']['tick'])<=m['tick_observed'] for x in m['messages'])
PY
cp -a "$root/direct" "$root/corrupt"
printf x >> "$root/corrupt/state.ck"
reject "$elf" inspect "$root/corrupt"
"$elf" init "$root/empty" "$source_dir/initial-fr.txt" 0 73 > /dev/null
cp "$root/empty/state.ck" "$root/empty-before"
"$elf" run "$root/empty" 100 0 > /dev/null
cmp "$root/empty/state.ck" "$root/empty-before"
"$elf" run "$root/split" 0 10 > /dev/null &
child=$!
sleep 0.15
reject "$elf" run "$root/split" 1 0
kill -TERM "$child"
wait "$child"
child=
"$elf" inspect "$root/split" > /dev/null
"$elf" run "$root/split" 1 0 > /dev/null
python3 - "$source_dir" "$root" <<'PY'
import importlib.util,pathlib,sys,json
spec=importlib.util.spec_from_file_location('broker',pathlib.Path(sys.argv[1])/'broker.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
for slug in ('https://evil.example','../secret','a%2fb','foo?bar','a/b',''):
 try:b.get_article(slug)
 except ValueError:pass
 else:raise AssertionError(slug)
bridge=pathlib.Path(sys.argv[2])/'broker';bridge.mkdir()
body=b'official fixture bytes\n'
def fake(slug):return body,{'url':'https://grokipedia.com/page/'+slug,'sha256':b.hashlib.sha256(body).hexdigest(),'bytes':len(body)}
b.get_article=fake
(bridge/'0.req').write_text('64\t9\t1\tArtificial_life\n')
s=b.process(bridge);assert s['accepted']==1;assert (bridge/'0.book').read_bytes()==body
(bridge/'1.req').write_text('64\t9\t2\thttps://evil.example\n')
s=b.process(bridge);assert s['rejected']==1;assert not (bridge/'1.book').exists()
print('BROKER PASS: strict slug allowlist, bounded import, provenance, invalid URLs rejected')
PY
printf '%s\n' 'CLI PASS: exact replay across checkpoint, mail provenance and commit cursor, corruption rejection, exclusive writer, SIGTERM, extinction without reseeding'
