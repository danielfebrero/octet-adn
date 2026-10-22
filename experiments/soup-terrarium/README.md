# soup-terrarium : on branche la soupe sur le terrarium

Question : est-ce que des réplicateurs **nés du bruit** dans la soupe (`./octet-adn soup`), une fois devenus fondateurs du terrarium, donnent un monde plus ouvert que des fondateurs conçus à la main ?

**Verdict (60 000 ticks, 4 îles, mesures ci-dessous) : non, ça plafonne.** Ce qui est vraiment nouveau, c'est la **viabilité**. Les réplicateurs de la soupe sont les seuls fondateurs qui tiennent en reproduction par copie réelle : 6 seeds sur 10, contre 0 sur 4 pour du bruit non sélectionné et 0 sur 4 pour le copieur écrit à la main. Ils s'installent en écologie, avec 4 à 6 niches stables. La sélection les rend encore plus copieurs (copier_frac 0.86 → 0.98), mais leur nouveauté est **inférieure** à celle de leur ombre neutre et elle décroît. Le canal social ne paie pas chez eux, et les prédateurs les exterminent.

## Pipeline

1. **Phase 1, la soupe.** `./octet-adn soup --addr b --programs 16384 --ticks 512 --report 256 --seed 2 --dump soup.bin`
   - Émergence à l'époque 256.
   - 15 910 copieurs sur 16 384 en fin de run.
   - Le dominant compte 7 octets essentiels.
   - `--dump` (nouveau) écrit les N × 64 octets de la soupe finale.
   - Le terrarium tire ses fondateurs uniformément dans ce dump, donc les réplicateurs dominants ET la soupe environnante. 96 % des fondateurs sont des copieurs vérifiés.
2. **Phase 2, le pont sémantique.** J'ai choisi **(a) la copie réelle** : `--repro copy`.
   - SPLIT gratuit est désactivé.
   - Un individu qui a ≥ 70 d'énergie et 64 octets exécute `soup_exec`, la VM de la soupe, sur `[soma ‖ 64 octets de bruit]` avec 8192 pas, adressage b, dans un ordre aléatoire. Ça coûte 2 d'énergie.
   - Un enfant naît seulement si le critère de naissance de la soupe est rempli : ≥ 8 octets écrits ET ≥ 56/64 identiques au programme, plus une mutation de 0.5 %/octet.
   - Raison : c'est la seule option où la reproduction reste *leur* capacité. Avec SPLIT, n'importe quel octet se reproduit, et l'origine soupe n'a plus de sens. Le contrôle le montre : random-split vit très bien.
   - L'hybride (b) est mesuré via `soup-split-osmo`, c'est-à-dire des fondateurs soupe avec SPLIT.
   - **Le pont métabolique `--osmo 1`.** Les programmes de soupe n'ont jamais eu à manger. Sans pont, **tous** les runs en copie s'éteignent :
     - fondateurs soupe à t = 600 ;
     - fondateurs random à t = 600–1800 ;
     - fondateurs handmade à t = 200–400.
   - C'est un résultat en soi. L'osmotrophie absorbe passivement ≤ 1 unité de nourriture de la case par tick (5 d'énergie). « ate » continue de mesurer un vrai EAT.
3. **Phase 3, le canal payant (optionnel).** `--pred K` ajoute K prédateurs par île.
   - Ils se déplacent vers la zone 3 × 3 la plus dense (p = 0.7) et infligent `--pred-dmg` d'énergie par tick à distance ≤ 1.
   - Ils écrivent 15 dans le nibble haut de env. Le vrai env est tiré dans 0..14, donc 15 = alarme sensorielle honnête.
   - Le contrôle `--channel off` coupe la perception de l'émission des voisins.
4. **L'ombre neutre au niveau composant.** Une population fantôme de génomes subit les mêmes morts et les mêmes naissances que la vraie :
   - même parent ;
   - même nombre de substitutions que la distance de Hamming réelle enfant/parent en copie, ou `mutate()` en split ;
   - sans sélection.
   - `scomp_*` compte ses composants (BP), comme `comp_*` pour la population réelle.
   - Elle a ses propres RNG, donc la dynamique d'origine est inchangée : le hash selftest reste 18547e045a455586.

## Résultats (60 000 ticks, seed 1 sauf mention ; quartile final Q4, entre parenthèses Q1)

| run | fondateurs / repro | survie | pop | H_geno | comp_new/10k réel | ombre_new/10k | niches stables (dernière apparition) | excès social Q4 (Q1) |
|---|---|---|---|---|---|---|---|---|
| soup-copy-s1 | soupe / copie + osmo | **6/10 seeds** | 1745 | 5.30 (6.48) | 115 (275) | 509 (851) | 6 (t = 6200) | −0.002 (+0.020) |
| soup-copy-chanoff | soupe / copie + osmo, canal off | oui | 1209 | 7.82 | 197 (239) | 85 | 6 (t = 21800) | −0.001 |
| random-copy | bruit / copie + osmo | **0/4** (t ≤ 3600) | 0 | – | – | – | – | – |
| handmade-copy | copieur conçu / copie + osmo | **0/4** (t ≤ 2600) | 0 | – | – | – | – | – |
| soup-split-osmo (hybride b) | soupe / SPLIT + osmo | oui | 565 | 9.07 | 2297 (4330) | 28139 | 4 (t = 10000) | +0.003 (+0.018) |
| seeds-split | conçus / SPLIT (terrarium actuel) | oui | 3478 | 11.76 | 4587 (10084) | 8023 | 6 (t = 9400) | +0.001 (+0.054) |
| seeds-split-osmo | conçus / SPLIT + osmo | oui | 2918 | 11.51 | 15347 (10913) | 12369 | 7 (t = 12800) | +0.001 (+0.067) |
| random-split-osmo | bruit / SPLIT + osmo | oui | 3389 | 11.70 | 2497 (7236) | 6495 | 8 (t = **35000**) | −0.000 (+0.028) |
| seeds-split-pred | conçus / SPLIT, 4 prédateurs | oui | 3597 | 11.81 | 24519 (18149) | 16077 | 7 (t = 14400) | +0.001 (+0.064) |
| seeds-split-pred-chanoff | idem, canal off | oui | 3644 | 11.83 | 19546 (17930) | 13521 | 9 (t = 15400) | +0.001 (−0.000) |
| soup-copy-pred (K = 4, dmg 25 / 10 / 5) | soupe / copie + osmo | **éteint** t = 1000–1400 | 0 | – | – | – | – | – |

Lecture :
- **Viabilité, le résultat net.**
  - Avec la copie réelle, seuls les fondateurs nés de la soupe passent le goulot de fondation : 6/10, contre 0/8 pour les autres.
  - Le goulot est réel. Après un boom initial (523 naissances sur 200 ticks), l'épuisement local de la nourriture fait chuter les naissances. 4 seeds sur 10 meurent à t = 1400–5000.
  - Le copieur handmade est un copieur parfait dans la soupe, mais il ne se déplace pas : il épuise sa case et meurt.
  - Les programmes de soupe contiennent assez d'octets de mouvement, et certaines niches (« mange = 1 ») font même de vrais EAT.
- **Pas d'ouverture.**
  - Dans soup-copy, la nouveauté réelle décroît : 275 → 115 par 10k ticks, soit 0.38 normalisée par naissance au Q4.
  - Elle reste 3 à 4× **sous** l'ombre neutre (509). La sélection purifie, elle n'invente pas.
  - H_geno baisse (6.48 → 5.30), copier_frac monte (0.86 → 0.98), les niches sont figées après t = 6200.
  - C'est un plateau de réplicateurs optimisés pour copier.
- **Parasites / réplicateurs plus courts.** min_ess oscille entre 6 et 11 (le dominant de la soupe en a 7). Il n'y a pas de raccourcissement durable.
  - Les parasites au sens Tierra sont structurellement impossibles ici : la matière première de la copie est du bruit frais, pas le code d'un voisin.
  - Je n'en ai pas vu.
- **Signal social.**
  - Aucun signal persistant nulle part : l'excès MI au Q4 est ≤ +0.003 bits/paire partout.
  - Des pics précoces existent seulement avec émission (fenêtres > 0.05 jusqu'à t ≈ 10 000 dans seeds-split-pred, 0 avec canal off).
  - Les prédateurs augmentent la nouveauté en mode split (24.5k contre 19.5k par 10k avec canal off, contre 4.6k sans prédateurs), mais ne rendent pas le signal persistant.
  - Chez les réplicateurs de soupe, aucune niche n'émet, et les prédateurs (même à dmg 5) les exterminent en ≤ 1400 ticks : ils n'ont aucun comportement de fuite.
- **Cycles.** Aucun cycle détecté : pas d'événement de type cycle, et des niches perdues sans retour.
- **Nouvelles niches tardives.** Seulement random-split-osmo (t = 35 000). Les fondateurs soupe se figent tôt.

Limites :
- 60k ticks et une seule soupe (seed 2). Le recensement de survie porte sur 10 seeds terrarium, mais les autres lignes reposent sur un seul seed chacune.
- La différence soup-copy-s1 / chanoff (H_geno 5.3 contre 7.8) est sur un seul seed : bruit possible.
- L'ombre en copie dérive vers peu de génomes (scomp_alive ≈ 5 dans chanoff), parce que les distances de copie sont petites. La comparaison réel/ombre y est moins informative.

## Reproduire

```sh
make && make experiments/terrarium_run
./octet-adn soup --addr b --programs 16384 --ticks 512 --report 256 --seed 2 --dump /tmp/soup.bin    # ~ quelques s
T=experiments/terrarium_run; S=soup:/tmp/soup.bin
$T run --dir runs/soup-copy --repro copy --osmo 1 --founders $S --seed 1 --ticks 60000            # ~ 2-3 min
$T run --dir runs/soup-copy-off --repro copy --osmo 1 --founders $S --seed 1 --channel off --ticks 60000
$T run --dir runs/random-copy --repro copy --osmo 1 --founders random --seed 1 --ticks 60000      # s'éteint
$T run --dir runs/hand-copy --repro copy --osmo 1 --founders handmade --seed 1 --ticks 60000      # s'éteint
$T run --dir runs/soup-copy-noosmo --repro copy --founders $S --seed 1 --ticks 60000              # s'éteint (pas de pont)
$T run --dir runs/soup-split --osmo 1 --founders $S --seed 1 --ticks 60000                        # hybride (b)
$T run --dir runs/seeds-pred --pred 4 --seed 1 --ticks 60000 ; $T run --dir runs/seeds-pred-off --pred 4 --channel off --seed 1 --ticks 60000
$T run --dir runs/soup-pred --repro copy --osmo 1 --founders $S --pred 4 --seed 1 --ticks 60000   # s'éteint
$T report runs/soup-copy
```

En continu : relancer la même commande avec `--ticks 0` (sans fin). `run` reprend tout seul depuis `D/state.ck` (checkpoint toutes les 2000 ticks, format OADNTER3 qui inclut l'ombre, les prédateurs et les RNG de copie). C'est déterministe : le selftest vérifie qu'un run d'un seul tenant est identique au même run repris, y compris en copie + prédateurs + ombre.

```sh
$T run --dir runs/soup-copy --repro copy --osmo 1 --founders $S --seed 1 --ticks 0     # Ctrl-C quand on veut ; puis $T report runs/soup-copy
```

Options ajoutées :
- `--founders seeds|random|handmade|soup:FICHIER`
- `--repro split|copy`
- `--rep-steps` (8192 ; à 2048 pas, seuls 2.3 % des programmes de soupe copient, contre 96.5 % à 8192), `--rep-cost`, `--rep-energy`, `--rep-p`, `--rep-mut`
- `--osmo`
- `--pred`, `--pred-dmg`, `--pred-every`

Nouvelles colonnes de series.tsv : `rep_try rep_ok copier_frac min_ess pred_hits scomp_cum scomp_new scomp_alive`.
