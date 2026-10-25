#!/bin/sh
# Démarre seulement de NOUVEAUX jardins vivants (rencontres entre mondes). N'arrête ni ne remplace rien.
# Les mondes sources sont lus une fois, en lecture seule, à la fondation sur l'hôte ; le conteneur ne voit
# que le dépôt en lecture seule et les dossiers des jardins.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
elf=$1                                   # relatif au dépôt, ex. experiments/transplant-build/<commit>/transplant
image=sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587
name=octet-jardins
test -x "$repo/$elf"; docker image inspect "$image" >/dev/null
if docker container inspect "$name" >/dev/null 2>&1; then echo "$name existe : aucun remplacement" >&2; exit 1; fi
j1=vie-jardin-senescence-parente-20261025; j2=vie-jardin-monde-forces-20261025
for j in $j1 $j2; do test ! -e "$repo/experiments/$j"; done
umask 002; ulimit -c 0
cd "$repo/experiments"
TP_CK=20000 "$repo/$elf" jardin vie-senescence-3a1923e vie-senescence-3a1923e vie-parente-3a1923e "$j1" 200 101 32
TP_CK=20000 "$repo/$elf" jardin vie-monde vie-monde vie-forces "$j2" 200 101 64
commit=$(git -c safe.directory="$repo" -C "$repo" rev-parse HEAD)
for j in $j1 $j2; do
  printf '{\n  "jardin": "%s",\n  "elf": "%s",\n  "elf_sha256": "%s",\n  "commit": "%s",\n  "graine": 101,\n  "checkpoint": 20000,\n  "cree": "%s"\n}\n' \
    "$j" "$elf" "$(sha256sum "$repo/$elf" | cut -d' ' -f1)" "$commit" "$(date -u +%FT%TZ)" > "$j/manifest.json"
  chmod -R g+w "$j"
done
docker run -d --name "$name" --restart on-failure:3 \
    --user 10003:999 --cpus 0.3 --memory 512m --pids-limit 32 \
    --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --mount "type=bind,src=$host_repo,dst=/app,readonly" \
    --mount "type=bind,src=$host_repo/experiments/$j1,dst=/data/$j1" \
    --mount "type=bind,src=$host_repo/experiments/$j2,dst=/data/$j2" \
    --workdir /app --entrypoint /bin/sh "$image" \
    -c "/app/$elf vivre /data/$j1 & /app/$elf vivre /data/$j2 & wait"
