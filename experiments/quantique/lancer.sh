#!/bin/sh
# Starts only new Q1 containers. Never stops/replaces any existing container.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
elf=experiments/quantique-build/20261025-q1/monde
image=sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587
test -x "$repo/$elf"
docker image inspect "$image" >/dev/null
python3 - "$repo" "$elf" <<'PY'
import csv,hashlib,json,sys
from pathlib import Path
r=Path(sys.argv[1]);p=r/'experiments/quantique/resultats-20261025'
m=json.loads((p/'manifest.json').read_text())
assert m['runs']==20
assert m['binary_sha256']==hashlib.sha256((r/sys.argv[2]).read_bytes()).hexdigest()
rows=list(csv.DictReader((p/'resultats.tsv').open(), delimiter='\t'))
assert len(rows)==20 and all(int(x['t'])==4096 and float(x['erreur_norme'])<1e-7 for x in rows)
assert all(int(x['projections'])==0 for x in rows if int(x['period'])==0)
PY
for p in 4 16 64; do
    if docker container inspect "octet-quantique-p$p" >/dev/null 2>&1; then
        echo "octet-quantique-p$p existe : aucun remplacement" >&2; exit 1
    fi
    test ! -e "$repo/experiments/vie-quantique-p$p-20261025"
done
for p in 4 16 64; do
    world=experiments/vie-quantique-p$p-20261025
    mkdir "$repo/$world"
    python3 - "$repo" "$world" "$elf" "$image" "$p" <<'PY'
import hashlib,json,subprocess,sys
from datetime import datetime,timezone
from pathlib import Path
r=Path(sys.argv[1]);d=r/sys.argv[2]
record={'created_at':datetime.now(timezone.utc).isoformat(),'seed':101,'period':int(sys.argv[5]),
        'octets':64,'modes':256,'grid':4,'dt':0.02,'image':sys.argv[4],
        'elf':sys.argv[3],'pause_ms_per_128_ticks':1000,'cpus':0.15,
        'commit':subprocess.check_output(['git','-c',f'safe.directory={r}','-C',str(r),'rev-parse','HEAD'],text=True).strip(),
        'sha256':{f:hashlib.sha256((r/f).read_bytes()).hexdigest() for f in
                  [sys.argv[3],'experiments/quantique/monde.cpp','experiments/quantique/PROTOCOLE.md']}}
(d/'manifest.json').write_text(json.dumps(record,indent=2)+'\n')
PY
    # Initialization is bounded and checkpointed before Docker starts. The
    # live command resumes without config flags, including after a crash.
    "$repo/$elf" run "$repo/$world" --ticks 1 --seed 101 --period "$p" >/dev/null
    docker run -d --name "octet-quantique-p$p" --restart on-failure:3 \
        --user 10003:999 --cpus 0.15 --memory 96m --pids-limit 32 \
        --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
        --mount "type=bind,src=$host_repo,dst=/app,readonly" \
        --mount "type=bind,src=$host_repo/$world,dst=/data" \
        --workdir /app --entrypoint "/app/$elf" "$image" \
        run /data --ticks 0 --pause-ms 1000
done
