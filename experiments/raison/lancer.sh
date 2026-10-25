#!/bin/sh
# Démarre seulement de NOUVEAUX mondes vivants « raison » : les 10 milieux, bras info, graine 101.
# Un conteneur à part (1 CPU pour les dix, sans réseau, dépôt en lecture seule). N'arrête ni ne remplace rien.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
host_repo=/data/eternalgrok-grok-workspace/produits/octet-adn
elf=$1                                   # chemin relatif au dépôt, ex. experiments/raison-build/2b003cf/raison
image=sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587
name=octet-raison
taches="memoire composition inversion observation patience chainage transfert signal autrui relation"
test -x "$repo/$elf"
docker image inspect "$image" >/dev/null
if docker container inspect "$name" >/dev/null 2>&1; then echo "$name existe : aucun remplacement" >&2; exit 1; fi
for t in $taches; do test ! -e "$repo/experiments/vie-raison-$t-20261025"; done
(cd "$repo" && "$elf" selftest >/dev/null)
umask 002
commit=$(git -c safe.directory="$repo" -C "$repo" rev-parse HEAD)
for t in $taches; do
  d="$repo/experiments/vie-raison-$t-20261025"
  # Fondation sur l'hôte (graines du dépôt), un cycle, checkpoint ; le conteneur reprend sans configuration.
  (cd "$repo" && "$elf" run "$d" --tache "$t" --bras info --graine 101 --cycles 1 --serie 1024 --eval 16384 --checkpoint 32768 >/dev/null)
  printf '{\n  "tache": "%s",\n  "bras": "info",\n  "graine": 101,\n  "elf": "%s",\n  "elf_sha256": "%s",\n  "commit": "%s",\n  "serie": 1024,\n  "eval": 16384,\n  "checkpoint": 32768,\n  "pause_ms_par_64_cycles": 1000,\n  "cree": "%s"\n}\n' \
    "$t" "$elf" "$(sha256sum "$repo/$elf" | cut -d' ' -f1)" "$commit" "$(date -u +%FT%TZ)" > "$d/manifest.json"
  chmod -R g+w "$d"
done
mounts=""
for t in $taches; do mounts="$mounts --mount type=bind,src=$host_repo/experiments/vie-raison-$t-20261025,dst=/data/$t"; done
cmd='for t in '"$taches"'; do /app/'"$elf"' run /data/$t --cycles 0 --pause-ms 1000 & done; wait'
docker run -d --name "$name" --restart on-failure:3 \
    --user 10003:999 --cpus 1.0 --memory 256m --pids-limit 32 \
    --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --mount "type=bind,src=$host_repo,dst=/app,readonly" \
    $mounts \
    --workdir /app --entrypoint /bin/sh "$image" -c "$cmd"
