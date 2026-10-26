#!/bin/sh
# Démarre le service relief-stockage (recompression sans perte gz -> xz de l'histoire fermée). Ne remplace rien.
set -eu
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
name=relief-stockage
if docker container inspect "$name" >/dev/null 2>&1; then echo "$name existe : aucun remplacement" >&2; exit 1; fi
docker run -d --name "$name" --restart unless-stopped \
  --user 10003:999 --cpus 0.5 --memory 1g --pids-limit 16 \
  --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
  -e HISTOIRE=/histoire -e AGE_HEURES=24 -e PAR_CYCLE=6 -e PAUSE_S=600 -e ALERTE_GO=50 -e PYTHONDONTWRITEBYTECODE=1 \
  --mount "type=bind,src=$host_repo/experiments/relief-20261025/stockage,dst=/w,readonly" \
  --mount "type=bind,src=$host_repo/experiments/vie-relief-20261025/histoire,dst=/histoire" \
  --entrypoint nice python:3.11-slim-bookworm -n 10 python3 -u /w/stockage.py
