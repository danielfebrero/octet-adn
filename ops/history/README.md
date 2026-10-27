# Historique intégral sur GitHub Releases

Ordre Daniel du 27/10/2026 : exporter automatiquement l'historique de toutes les expériences Octet-ADN, y compris les prochains mondes, et libérer le disque après vérification.

## Services installés

`launch.sh` installe deux services Docker `restart=unless-stopped`, indépendants des mondes vivants. Aucun monde n'est redémarré.

- `octet-history-archive` : inventorie chaque heure **tous** les fichiers ordinaires sous `experiments/` et `labs_data/`, ainsi que les sauvegardes `backups/octet-adn-*`. Les nouveaux dossiers sont découverts sans liste de mondes à maintenir. Les changements sont réexportés ; une version observée pendant une écriture est réessayée au cycle suivant. Pas de copie de `.git`, caches Python, verrous, temporaires, PID ni liens symboliques. L'archive froide possède aussi son `git.bundle`.
- `octet-history-capture` : relève toutes les 30 secondes les versions des anneaux `experiments/vie-*/reports/*.json` dans un spool durable. Évite leur écrasement avant le prochain export horaire, sans redémarrer Passage. Une panne du collecteur pendant plus d'un tour complet de l'anneau peut perdre des versions : ce n'est pas une garantie de capture des états RAM ni de chaque checkpoint.
- Les **prochains runs du générateur** écrivent chaque rapport validé sous `histoire/reports/NUMERO.json`, en plus de l'anneau local. Ces fichiers immuables sont exportés puis retirés. Le Passage déjà lancé garde son code en mémoire et est couvert par le collecteur.
- `relief-releases` continue d'exporter les segments `events-*` de Relief avec son verrou partagé. Le nouvel archiveur ne les manipule pas ; il couvre aussi les séries fermées.

Créer les prochains mondes et leurs données sous `experiments/` dans ce dépôt. Un monde hors de cet arbre requiert un montage et une racine supplémentaires ; ne pas lui promettre une couverture automatique.

## Format et vérification

Releases quotidiennes `octet-history-YYYYMMDD` du dépôt `danielfebrero/octet-adn`. Blocs de 16 Mio maximum, regroupés dans des TAR gzip d'environ 128 Mio bruts, limitant le tampon local. Les fichiers géants sont répartis sur plusieurs archives. Chaque JSON contient chemin d'origine, taille du fichier observé, signature de version, permissions, offsets et SHA-256 de chaque bloc, ainsi que SHA-256 du TAR. Compression sans perte, aucun résumé des octets.

Chaque TAR **et** chaque manifeste est téléchargé intégralement depuis GitHub et vérifié. Le journal est synchronisé puis l'index SQLite validé **avant** toute suppression. Les blocs de l'original sont relus et comparés avant retrait ; un changement concurrent empêche le retrait. Échec réseau, SHA incorrect ou export incomplet : conservation locale et `ERROR`.

Les snapshots des fichiers vivants sont indépendants : ils ne constituent pas un checkpoint transactionnel de plusieurs habitats. Les temporaires de transactions ne sont pas une source de reprise.

## Rétention locale

- États courants, ELF, paramètres, recettes, lois, bibliothèques, files d'injection, journaux ouverts et anneaux : conservés. Aucun `copytruncate`, aucun arrêt ou réensemencement.
- Mondes froids explicitement archivés le 25/10 : `cold-worlds.json` (44 répertoires, contrôles inclus). Leurs fichiers partent après vérification. Un futur monde n'est **jamais** déclaré froid par son âge.
- Sauvegarde froide explicitement autorisée `octet-adn-20261025-20f62d86` : fichiers déplacés vers Releases après vérification ; les autres sauvegardes sont exportées sans éviction automatique.
- Rapports immuables `histoire/reports` et spool du collecteur : retrait après vérification.
- Segments d'histoire datés fermés : 24 heures locales ; Relief conserve sa règle particulière sur les événements.
- Snapshots historiques : sept jours locaux, puis retrait vérifié.
- Courrier : sept jours minimum ; retrait seulement des séquences déjà ramassées (`pickups/cursor`), après export vérifié. Originaux anciens récupérables depuis Releases. Ne pas remettre le curseur à zéro sans restaurer le courrier correspondant.

Le journal local tourne à 8 Mio ; ses segments fermés sont aussi exportés puis retirés. L'index ne conserve que les versions encore utiles localement.

Les journaux ouverts restent locaux. Les nouvelles implémentations doivent fermer et segmenter leurs journaux à taille bornée (modèle : `relief-20261025/vivre.sh`) et placer les segments sous `histoire/`. Un writer qui garde un fichier ouvert indéfiniment ne peut être purgé sans son protocole de rotation.

## Restauration

```sh
python3 ops/history/restore.py \
  repo/experiments/vie-monde/state.ck /chemin/neuf/state.ck
```

L'outil parcourt les manifestes des Releases quotidiennes, sélectionne la dernière version **complète**, télécharge les TAR nécessaires, vérifie tous les SHA-256 et recompose exactement les blocs. Il refuse d'écraser une destination existante. Toujours restaurer dans un dossier séparé ; ne pas relancer les mondes éteints.

Les chemins des sauvegardes commencent par `backups/octet-adn-20261025-20f62d86/`. Les rapports capturés de Passage sont sous `captured/vie-passage-001-20261027/NUMERO-SHA.json` ; leur champ `batches` identifie le lot d'origine.

## Exploitation

Le service d'export utilise UID 0 avec la seule capacité `DAC_OVERRIDE`, pour lire les anciens fichiers privés des différents runners ; les autres capacités sont supprimées. Le collecteur est UID 10003, sans réseau. Les montages du jeton et du code restent hors du périmètre archivé.

État durable : `/workspace/ops/octet-history/` (`index.sqlite`, `releases.jsonl`, spool et marqueurs). Le jeton n'est jamais copié dans cet état ni les archives : montage en lecture seule depuis le coffre hôte.

```sh
docker logs --tail 20 octet-history-archive
docker logs --tail 10 octet-history-capture
docker inspect octet-history-archive --format '{{.State.Status}} {{.RestartCount}}'
python3 -m unittest discover -s ops/history -p 'test_*.py' -v
```

Une indisponibilité de GitHub conserve les données sur le VPS et peut faire augmenter le disque : contrôler `ERROR`, dernier `cycle_ok` et espace libre à chaque regard des mondes. Ne jamais contourner une erreur par une purge non vérifiée. GitHub reste une dépendance externe ; les données déjà écrasées avant l'installation ne sont pas recréées.
