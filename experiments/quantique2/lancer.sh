#!/bin/sh
# Démarre seulement un NOUVEAU monde Q2 vivant (bras E, réglage préenregistré). N'arrête ni ne remplace rien.
# Même isolation que Q1 : conteneur à part, 0,15 CPU, sans réseau, dépôt en lecture seule.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
elf=experiments/quantique-build/20261025-q2v/monde2
image=sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587
name=octet-quantique2-e
world=experiments/vie-quantique2-e-20261025
seed=101
test -x "$repo/$elf"
docker image inspect "$image" >/dev/null
if docker container inspect "$name" >/dev/null 2>&1; then echo "$name existe : aucun remplacement" >&2; exit 1; fi
test ! -e "$repo/$world"
"$repo/$elf" selftest
umask 002
mkdir "$repo/$world"
commit=$(git -c safe.directory="$repo" -C "$repo" rev-parse HEAD)
{
  printf '{\n  "created_at": "%s",\n  "seed": %s,\n  "bras": "E",\n' "$(date -u +%FT%TZ)" "$seed"
  printf '  "reglage": "--imprint 1 --cool 100 --n 96 --box 12 --grid 6 --modes 64 --period 16 --temp 0.6 --gamma 0.5 --rep 1.5 --xi 2 --rcut 5 --spec 0.5 --gain 1 --bind-r 1.4 --window 1024",\n'
  printf '  "image": "%s",\n  "elf": "%s",\n  "elf_sha256": "%s",\n' "$image" "$elf" "$(sha256sum "$repo/$elf" | cut -d" " -f1)"
  printf '  "source_sha256": "%s",\n' "$(sha256sum "$repo/experiments/quantique2/monde2.cpp" | cut -d" " -f1)"
  printf '  "cpus": 0.15,\n  "pause_ms_per_128_ticks": 1000,\n  "journal_projections": false,\n  "commit": "%s"\n}\n' "$commit"
} > "$repo/$world/manifest.json"
# Initialisation bornée et checkpointée avant Docker ; le conteneur reprend sans configuration.
"$repo/$elf" run "$repo/$world" --ticks 1 --seed "$seed" --imprint 1 --cool 100 --n 96 --box 12 --grid 6 --modes 64 \
    --period 16 --temp 0.6 --gamma 0.5 --rep 1.5 --xi 2 --rcut 5 --spec 0.5 --gain 1 --bind-r 1.4 --window 1024 >/dev/null
chmod g+w "$repo/$world"/*
docker run -d --name "$name" --restart on-failure:3 \
    --user 10003:999 --cpus 0.15 --memory 96m --pids-limit 32 \
    --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --mount "type=bind,src=$host_repo,dst=/app,readonly" \
    --mount "type=bind,src=$host_repo/$world,dst=/data" \
    --workdir /app --entrypoint "/app/$elf" "$image" \
    run /data --ticks 0 --pause-ms 1000 --events 0
