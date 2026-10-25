#!/bin/sh
# Instance distincte ; ne stoppe/remplace aucun conteneur ni ELF existant.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
world=experiments/vie-choix-e1-20261025
elf=experiments/choix-build/20261025-e2/choix
image=sha256:2cfcd3ab219f91d00877f41e8f46ff7068f35498f457b4b5d68a06e67bd4d179
if docker container inspect octet-choix-e1 >/dev/null 2>&1; then
    echo 'octet-choix-e1 existe : examiner son état, aucun remplacement automatique' >&2
    exit 1
fi
test -x "$repo/$elf"
python3 - "$repo" <<'PY'
import json, sys
from pathlib import Path
r=Path(sys.argv[1]);p=r/'experiments/choix/resultats-20261025'
x=json.loads((p/'resultats.json').read_text())
assert x['M1']['device_valid'] and x['M2']['device_valid']
PY
mkdir "$repo/$world"
python3 - "$repo" "$world" "$elf" "$image" <<'PY'
import hashlib,json,subprocess,sys
from datetime import datetime,timezone
from pathlib import Path
r=Path(sys.argv[1]);d=r/sys.argv[2]
files=[sys.argv[3],'experiments/choix.c','src/vm.c','src/isa.c','src/rng.c',
       'include/octet.h','experiments/choix/ECOLOGIE.md','experiments/choix/lancer-e1.sh',
       'seeds/general.adn','seeds/echo.adn','seeds/neural.adn','seeds/forager.adn']
record={'created_at':datetime.now(timezone.utc).isoformat(),'seed':20261025,
        'image':sys.argv[4],'elf':sys.argv[3],
        'commit':subprocess.check_output(['git','-c',f'safe.directory={r}','-C',str(r),'rev-parse','HEAD'],text=True).strip(),
        'files':{f:hashlib.sha256((r/f).read_bytes()).hexdigest() for f in files},
        'founders':64,'max_pop':160,'pause_ms':1000,
        'note':'survie/reproduction ; aucun oracle/T8 introduit, aucune émergence revendiquée'}
(d/'manifest.json').write_text(json.dumps(record,indent=2)+'\n')
PY
docker run -d --name octet-choix-e1 --restart on-failure:3 \
    --user 10003:999 --cpus 0.5 --memory 256m --pids-limit 64 \
    --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --tmpfs /tmp:rw,noexec,nosuid,size=8m \
    --mount "type=bind,src=$host_repo,dst=/app,readonly" \
    --mount "type=bind,src=$host_repo/$world,dst=/data" \
    --workdir /app --entrypoint "/app/$elf" "$image" ecology /data 0 1000
