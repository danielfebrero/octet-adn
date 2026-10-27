#!/bin/sh
# Idempotent service installation; no living-world container is restarted.
set -eu
repo=/data/eternalgrok-grok-workspace/produits/octet-adn
backups=/data/eternalgrok-grok-workspace/backups
state=/data/eternalgrok-grok-workspace/ops/octet-history
# Dedicated state directory, no token is ever copied into it.
docker run --rm --network none --mount type=bind,src=/data/eternalgrok-grok-workspace/ops,dst=/ops alpine:3.20 sh -c 'mkdir -p /ops/octet-history; chown 10003:999 /ops/octet-history; chmod 2770 /ops/octet-history'
for role in capture archive; do
  name=octet-history-$role
  if docker container inspect "$name" >/dev/null 2>&1; then
    echo "$name already installed"; continue
  fi
  if [ "$role" = capture ]; then
    docker run -d --name "$name" --restart unless-stopped --user 10003:999 \
      --cpus 0.25 --memory 192m --pids-limit 16 --network none \
      --read-only --cap-drop ALL --security-opt no-new-privileges \
      -e PYTHONDONTWRITEBYTECODE=1 \
      --mount type=bind,src="$repo",dst=/repo,readonly \
      --mount type=bind,src="$state",dst=/state \
      --entrypoint python3 python:3.11-slim-bookworm -u /repo/ops/history/capture.py
  else
    docker run -d --name "$name" --restart unless-stopped --user 10003:999 \
      --cpus 1 --memory 768m --pids-limit 16 \
      --read-only --cap-drop ALL --security-opt no-new-privileges \
      -e PYTHONDONTWRITEBYTECODE=1 \
      --mount type=bind,src="$repo",dst=/repo \
      --mount type=bind,src="$backups",dst=/backups \
      --mount type=bind,src="$state",dst=/state \
      --mount type=bind,src=/opt/eternalgrok-mcp/secrets/github_pat_codex,dst=/run/secrets/github_pat,readonly \
      --entrypoint python3 python:3.11-slim-bookworm -u /repo/ops/history/archive.py
  fi
done
