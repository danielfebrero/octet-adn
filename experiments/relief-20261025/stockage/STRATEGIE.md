# Stockage de Relief — stratégie (26 octobre 2026)

## Constat mesuré

- Seul flux qui grandit sans fin : `vie-relief-20261025/histoire/events-*.tsv.gz`, segments fermés et gzippés
  par `vivre.sh` à 32 Mio. **~2,1 Go/jour** (119 segments en 17,9 h). L'état (`state.ck`, 25 Mo) et la série
  (`series.tsv`, ~2 Mo/jour) sont négligeables.
- 70 % du volume brut = lignes de naissance, qui recopient le génome en hexadécimal ; 130 572 naissances pour
  48 107 génomes distincts dans un segment. Forte redondance, mal exploitée par gzip.
- Disque partagé `/dev/md3` : 878 Go, 290 Go libres au 26/10. Relief n'en occupe que 1,6 Go ; les gros postes sont
  ailleurs (dev, recherche, cache, tmp, mini-apps), hors de ce monde.

Essais sans perte sur un segment de 36 Mo brut : gzip actuel 13,3 Mo ; **xz -9 : 5,0 Mo (−62 %)** ; xz -6 : 5,3 Mo ;
dictionnaire des génomes + xz : 5,3 Mo (xz trouve déjà la redondance, pas de format maison).

## Niveau 1 — en place : recompression sans perte (service `relief-stockage`)

`stockage.py`, toutes les 10 min, au plus 6 segments par passage :
- seulement les `*.tsv.gz` fermés depuis **plus de 24 h** (le plus récent reste en gzip pour l'observatoire et les regards) ;
- écrit `NOM.tsv.xz.tmp`, le relit, compare la SHA-256 du contenu à celle du `.gz` ; **égal → renomme en `.tsv.xz`
  (date d'origine conservée), puis retire le `.gz`** ; différent → garde le `.gz`, journalise, s'arrête ;
- n'efface rien d'autre, ne résume rien ; un `.xz.tmp` laissé par un arrêt brutal est retiré au passage suivant ;
- conteneur à part : UID 10003, réseau coupé, rootfs en lecture seule, 0,5 CPU, 1 Gio ; seul `histoire/` est monté
  en écriture. Le monde vivant, son ELF, `state.ck`, `events.tsv` et `series.tsv` ne sont pas montés.

Effet attendu : stockage ≈ **0,45 × le gzip** (essai sur 3 segments : 31,1 Mo → 13,8 Mo, contenu identique vérifié aussi
avec `xz -t` et `xzcat` officiels), soit ~0,9–1 Go/jour au lieu de 2,1. Lecture : `zcat` pour `.gz`, `xzcat` pour `.xz`.

## Niveau 2 — en place : surveillance

`histoire/STOCKAGE.txt` (réécrit à chaque passage) : disque libre, taille de l'histoire, segments gz/xz, volume gz des
dernières 24 h, ratio xz/gz mesuré, rythme stocké, **jours restants**, `statut=ok` ou `ALERTE disque < 50 Go`.
`histoire/stockage.log` : une ligne par segment (tailles, SHA-256). Le regard de Relief lit ces deux fichiers.

## Niveau 3 — à décider par Daniel, jamais automatique

Seuils qui déclenchent une proposition : disque libre < 100 Go, ou histoire > 100 Go.
1. **Export à froid** des segments xz de plus de 30 jours hors du VPS (empreintes SHA-256 vérifiées des deux côtés),
   puis retrait local seulement après vérification.
2. **Allègement** si la donnée n'est plus utile : les interactions (dépôts, constructions, attaques, dons, parasitisme)
   sont déjà échantillonnées au 1/256 et pèsent ~10 % ; naissances et décès restent intégraux.
3. Ailleurs sur le disque (hors Relief) : `cache` 31 Go et `tmp` 24 Go sont les candidats naturels à un ménage, à voir
   avec leurs propriétaires ; l'archive des 34 anciens mondes (6,8 Go dans `backups`) et les séries d'expériences
   (1,3 Go) peuvent être recompressées en xz sans perte.

Aucune suppression de l'histoire de Relief n'est faite sans export vérifié et accord explicite.
