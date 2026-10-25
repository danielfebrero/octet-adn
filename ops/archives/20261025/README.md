# Relève des 34 mondes —25 octobre 2026

Ordre direct de Daniel `20f62d86-e1f4-41e9-a840-e0047756ad7c` : tous archiver et arrêter, puis nouveau monde plus riche dimensionné VPS. **34/34 reprises préflight et34/34 reprises finales réussies**, avec progression réelle sur copies et checkpoints sources inchangés. Les 34 anciens sont arrêtés ;7 conteneurs restart=no ; octet-mondes.service disabled/inactive. E1 demeure éteint, sans fondateurs.

## Archive sur le VPS

`/workspace/backups/octet-adn-20261025-20f62d86/`

- `repo/` : copie finale froide des sources, ELF exacts, checkpoints, journaux, paramètres et modules .so, E1 inclus.
- `preflight-repo/` : premier état gelé et testé avant arrêt.
- `containers.json` : commandes, montages, images et processus, sans environnement ni secrets.
- `superviseur/`, `git.bundle`, `historical-runtime/`, `runtime-image.tar` : superviseur retiré, historique Git, libc/libm/chargeur historiques, image quantique/raison/jardin exacte.
- `pre-stop-processes.json`, `stopped.json` : preuves de transition.
- `final-verify.jsonl`, `preflight-verify.jsonl`, `verify-reprise.py`, `verification-finale.json`, `AUDIT-ARCHIVAGE.md`, `elf-so-sha256.json` : essais et limites.
- `SHA256SUMS` : empreintes, à contrôler depuis le dossier d’archive.

Les originaux restent disponibles pour inspection. Les verrous éphémères rencontres.lock ne portent aucun état biologique et ne sont pas copiés. Aucun nettoyage des expériences.

## Arrêt et limites

18 terrariums+10 raison+2 jardins sortis code 0 après SIGTERM individuel. Les 4 Claude sont visés avant les 14 enfants du shell. Les 4 quantiques sont PID 1 sans handler et ignorent TERM : gel, copie du dernier checkpoint atomique, puis KILL (exit 137, sans OOM). Leur RAM finale n’est pas sauvegardée ; jusqu’à128 ticks depuis le checkpoint peuvent être perdus. La reprise tronque les journaux au tick checkpointé.

Les terrariums peuvent aussi perdre la dernière fraction non checkpointée. Tronc et Lecture ont des modules (`memoire.so`, `trace.so`) dont la RAM statique n’est pas dans state.ck : **reprise opérationnelle, pas image bit-identique de cette RAM ni garantie de même ordre de chargement**. Les essais valident chargement et progression, pas l’identité de chaque trajectoire contrefactuelle.

## Reprise volontaire et isolée

Ne pas relancer les anciens à un regard ni réactiver leur superviseur. Copier le monde depuis repo/ dans un NOUVEAU chemin inscriptible, avec son ELF et ses modules exacts. verify-reprise.py contient les commandes réellement testées : ticks terrarium=cible absolue, ticks quantiques=durée, raison cycles=durée, transplant vivre DIR N=durée. Recharger l’image par docker load -i runtime-image.tar si nécessaire. Pour le runtime historique : historical-runtime/ld-linux-x86-64.so.2 --library-path historical-runtime ELF ... . Ne jamais essayer sur une archive ni un original.

Le successeur est [Relief](../../../experiments/relief-20261025/README.md). Les anciennes fiches sont désormais historiques.
