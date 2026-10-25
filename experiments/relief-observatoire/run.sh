#!/bin/sh
set -eu
APP=/data/eternalgrok-grok-workspace/produits/octet-adn/experiments/relief-observatoire
WORLD=/data/eternalgrok-grok-workspace/produits/octet-adn/experiments/vie-relief-20261025
if [ -z "${TELEGRAM_BOT_TOKEN:-}" ]; then
 TELEGRAM_BOT_TOKEN=$(docker exec eternalgrok-eg-telegram printenv TELEGRAM_BOT_TOKEN)
fi
TELEGRAM_ALLOWED_USER_IDS=${TELEGRAM_ALLOWED_USER_IDS:-7460665424}
export TELEGRAM_BOT_TOKEN TELEGRAM_ALLOWED_USER_IDS
docker run -d --name relief-observatoire --restart unless-stopped \
 --network eternalgrok-mcp_eternalgrok --read-only --cap-drop ALL \
 --security-opt no-new-privileges --user 10001:999 --memory 256m --cpus 1 --pids-limit 32 \
 -v "$APP:/app:ro" -v "$WORLD:/world:ro" \
 -e TELEGRAM_BOT_TOKEN -e TELEGRAM_ALLOWED_USER_IDS -e PYTHONDONTWRITEBYTECODE=1 \
 -w /app python:3.11-slim-bookworm python3 -u server.py
