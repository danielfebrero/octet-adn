#!/bin/sh
# Rotation + compression des journaux d'événements des mondes quantiques vivants.
#
# Les ELF sont figés et écrivent events.tsv en O_APPEND ; on ne touche ni au processus
# ni à state.ck. Méthode copytruncate exécutée DANS le conteneur, sous l'uid du monde :
# gzip du journal vers events-archive/, puis truncate -s 0. Toute la donnée est gardée,
# compressée (~10x). Seule perte possible : les quelques lignes écrites pendant le gzip
# (sous-seconde). mesures.tsv, naissances.tsv et state.ck ne sont jamais touchés, et
# recover_log relit au redémarrage ce qui reste : la reprise est intacte.
#
# Réglages par variables d'environnement :
#   SEUIL            octets ; rotation seulement si events.tsv dépasse ce seuil (défaut 32 Mio)
#   RETENTION_JOURS  purge des archives gz plus vieilles que N jours (défaut 7)
set -eu
seuil=${SEUIL:-33554432}
jours=${RETENTION_JOURS:-7}

for name in octet-quantique-p4 octet-quantique-p16 octet-quantique-p64 octet-quantique2-e; do
  docker container inspect "$name" >/dev/null 2>&1 || { echo "absent, ignoré : $name"; continue; }
  docker exec -u 10003 -e SEUIL="$seuil" -e JOURS="$jours" "$name" sh -c '
    set -eu
    f=/data/events.tsv
    [ -f "$f" ] || exit 0
    taille=$(stat -c %s "$f")
    [ -d /data/events-archive ] && find /data/events-archive -name "events-*.tsv.gz" -mtime +"$JOURS" -delete 2>/dev/null || true
    if [ "$taille" -lt "$SEUIL" ]; then
      printf "%s octets < seuil, rien à faire\n" "$taille"; exit 0
    fi
    mkdir -p /data/events-archive
    tick=$(tail -n 1 "$f" | cut -f1); [ -n "$tick" ] || tick=0
    out=/data/events-archive/events-t${tick}-$(date -u +%Y%m%dT%H%M%SZ).tsv.gz
    gzip -c "$f" > "$out" || true
    truncate -s 0 "$f"
    printf "roté : %s octets -> %s (%s octets)\n" "$taille" "$out" "$(stat -c %s "$out")"
  ' 2>&1 | sed "s/^/$name: /"
done
