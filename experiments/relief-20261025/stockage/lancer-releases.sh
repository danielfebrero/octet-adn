#!/bin/sh
# Export horaire : seul le dossier histoire est inscriptible.
set -eu
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
name=relief-releases
if docker container inspect "$name" >/dev/null 2>&1; then echo "$name existe" >&2; exit 1; fi
docker run -d --name "$name" --restart unless-stopped \
  --user 10003:999 --cpus 0.5 --memory 256m --pids-limit 16 \
  --read-only --cap-drop ALL --security-opt no-new-privileges \
  -e HISTOIRE=/histoire -e PYTHONDONTWRITEBYTECODE=1 \
  --mount "type=bind,src=$host_repo/experiments/relief-20261025/stockage,dst=/w,readonly" \
  --mount "type=bind,src=$host_repo/experiments/vie-relief-20261025/histoire,dst=/histoire" \
  --mount type=bind,src=/opt/eternalgrok-mcp/secrets/github_pat_codex,dst=/run/secrets/github_pat,readonly \
  --entrypoint python3 python:3.11-slim-bookworm -u /w/releases.py
