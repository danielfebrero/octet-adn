# experiments/soup-replicator — un réplicateur émerge-t-il d'une soupe d'octets ?

Question : sans labo, sans fitness et **sans SPLIT** (aucune division offerte par le moteur),
un programme qui se recopie apparaît-il à partir de bruit uniforme, avec l'ISA d'octet-adn ?
Protocole inspiré de BFF (Agüera y Arcas et al., 2024, « Computational life ») et de Tierra.

## Dispositif (`octet-adn soup`, `src/soup.c`)

- N programmes de 64 octets, initialisés au hasard (octets uniformes 0..255 ; ≥ 0x20 = introns).
- À chaque époque : appariement aléatoire, concaténation en une bande de 128 octets, exécution de
  l'ISA existante depuis ip = 0, A = B = 0. Arrêt : ip sort de la bande, YIELD, ou budget de 8192 pas.
  SPLIT, capteurs et actions du monde sont sans effet. La bande est ensuite recoupée en deux programmes.
- Mutation de fond : 0,00024 par octet et par époque.
- Deux adressages pour LOAD/STORE k :
  - `abs` (sémantique d'origine) : `bande[k % 128]`.
  - `b` : `bande[(B + k) % 128]`. Avec B, une boucle INC_B peut parcourir la bande ; c'est l'équivalent des têtes de BFF.
- **Naissance** (le seul moyen de naître) : une moitié a ≥ 8 octets écrits et changés pendant l'interaction,
  et ≥ 56/64 octets identiques à l'autre moitié. Le moteur la détecte après coup ; il n'existe aucune instruction « copie ».
- **Réplicateur vérifié** : un programme qui fait une copie fidèle (≥ 56/64) sur 3 partenaires bruités sur 4, et dont la copie fait de même.
- **Ablation** : chaque octet reçoit 4 substitutions. Il est *essentiel* si au moins 3 d'entre elles cassent la copie.
  `min_essentiels` est le plus petit nombre d'octets essentiels parmi les réplicateurs échantillonnés.
- Mesures par fenêtre : naissances, programmes uniques, copieurs vérifiés et leurs variantes, k-mer de 8 le plus
  fréquent avec sa part, entropie des octets (bits).
- Les labos ne sont pas touchés : `test_core_state_hash_matches_main` reste identique et `make test` passe (40/40).
  Le test `test_soup_detector_controls` vérifie :
  - le cas témoin : les 2 copieurs faits main ont 22 et 9 octets essentiels ;
  - qu'avec `no_store`, rien n'est reconnu comme copieur ;
  - que du bruit pur n'est pas un copieur ;
  - le déterminisme ;
  - qu'une injection envahit la soupe, et qu'avec `no_store` il n'y a aucune naissance.

## Résultats (graines indépendantes ; sorties brutes dans `results.txt`)

| adressage | N | époques | graines | réplicateur émergent | t_first (époques) | min octets essentiels | naissances spontanées sans réplicateur |
|---|---|---|---|---|---|---|---|
| abs | 4096 | 16384 | 0–7 | **0/8** | — | — | 0 sur chaque graine |
| abs | 16384 | 16384 | 0–1 | **0/2** | — | — | 0 |
| b | 4096 | 16384 | 0–7 | **0/8** | — | — | 2, 2, 2, 2, 0, 3, 0, 3 |
| b | 16384 | 16384 (5120 / 2048 une fois l'émergence vue) | 0–3 | **2/4** | graine 0 : 5120 (entre 4864 et 5120) ; graine 2 : 256 (≤ 256) | 8 (g0), 5 (g2) | 10 (g1), 10 (g3) |

En `b`, N = 16384 :

- **Graine 0.**
  - Entre les époques 4864 et 5120, la soupe bascule. 16019/16384 programmes sont des copieurs vérifiés, en 373 variantes.
  - Le nombre de programmes uniques tombe de 16384 à 493. L'entropie passe de 5,72 à 3,89 bits.
  - Le k-mer dominant `0701030903c90690` couvre 6,6 % des positions.
  - La soupe est encore dominée à l'époque 12288 : 16131 copieurs, 50 088 198 naissances cumulées, min_ess 6 (course longue interrompue pour tenir le budget de calcul).
  - Le réplicateur dominant est un motif de 16 octets répété 4 fois : `05 1d 03 02 1c 1f ff 07 01 03 09 03 c9 06 90 1e`.
  - 8 octets sont essentiels (positions 1, 7–10 et 13–15) : `LOAD 1 / INC_B / JZ … / STORE 144 (= B+16) / LOOP (ip=0)`.
- **Graine 2.**
  - Le réplicateur est déjà là à l'époque 256 : 11680 copieurs. À l'époque 2048, il y en a 16235 et 13 702 635 naissances.
  - Le dominant n'a que **6 octets essentiels** : `04 … 07 40 … 06 00 … 1e`, soit `DEC_B / LOAD 64 / STORE 0 / LOOP`.
  - Les 58 autres octets restent du bruit libre. Le minimum observé dans la course est de 5 octets.
- **Graines 1 et 3** : 10 naissances isolées en 16384 époques, aucune lignée, entropie ≈ 5,6 bits.

En adressage `abs`, aucune naissance sur 10 graines.

Témoins :

- **Positif.** 16 copies du copieur fait main, N = 4096, 2048 époques, graines 0–3.
  - abs : 4022–4045 copieurs à la fin, min_ess 20–21.
  - b : 3297–4024 copieurs à la fin, min_ess 8–9.
  - L'invasion est visible dès l'époque 256 dans les 8 cas.
- **Négatif.** Avec `--no-store`, en abs comme en b : 0 naissance et 0 copieur en 4096 époques.

## Verdict

- **OUI, mais uniquement avec l'adressage relatif à B, et à partir de N = 16384.**
- Un réplicateur émerge spontanément du bruit sur **2 graines sur 4**, après **≤ 256** et **≈ 5000** époques.
- Il tient sur **5 à 8 octets nécessaires** (ablation) : une boucle LOAD/STORE indexée par B, plus LOOP.
- Il envahit la soupe : > 97 % de copieurs.
- **Non** avec la sémantique d'origine (`abs`) : 0/10 graines, 0 naissance. Copier y exige d'auto-modifier l'immédiat ; le copieur fait main a besoin de 20–22 octets essentiels.
- **Non** à N = 4096 : 0/8 en `b`, malgré des quasi-copies isolées.
- La taille de la soupe compte. BFF utilisait 2^17 programmes.

## Reproduire

```
make
./octet-adn soup --addr b --programs 16384 --ticks 2048 --report 256 --seed 2   # émergence <= 256 (~10-15 min, 1 cœur)
./octet-adn soup --addr b --programs 16384 --ticks 5120 --report 256 --seed 0   # émergence ~5120
./octet-adn soup --addr abs --programs 4096 --ticks 16384 --report 512 --seed 0  # négatif
./octet-adn soup --addr b --programs 4096 --ticks 2048 --report 256 --inject 16 --seed 0   # témoin positif
./octet-adn soup --addr b --programs 4096 --ticks 4096 --report 512 --no-store --seed 0    # témoin négatif
```

## Prochaine expérience

- Mesurer la probabilité d'émergence en fonction de N : 4096, 8192, 16384, 32768, sur ≥ 16 graines en `b`, pour estimer le seuil.
- Ensuite, lancer `abs` à 2^17 programmes : la sémantique d'origine admet-elle une émergence à l'échelle de BFF ?
- Puis lancer une course longue après émergence, pour savoir si des réplicateurs plus courts ou plus rapides remplacent le premier (course aux armements à la Tierra).
- Enfin, transplanter un réplicateur émergé dans les labos pour voir s'il y survit.
