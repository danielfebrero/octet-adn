# universe-search v2 : fitness robuste, origine à N=16384, dérive lente de la physique

Programme : `experiments/universe_search2.c`. Il inclut `universe_search.c` (v1) sans le modifier : même génome
d'univers, même soupe BFF, même copieur vérifié, mêmes garde-fous (catalogue de 28 primitives, aucune primitive
n'écrit plus d'un octet). `make test` lance `universe_search2 selftest`, qui vérifie les tests Fisher et 2000 pas de
dérive sans violation. Le hash du cœur est inchangé et `seeds/` n'est pas touché.

```
make experiments/universe_search2
./experiments/universe_search2 race   --seed 2 --save /tmp/fin2.bin                         # ~4 min, 8 cœurs
./experiments/universe_search2 origin --load /tmp/fin2.bin --N 16384 --epochs 2048 --seeds 8  # ~3 min
./experiments/universe_search2 drift  --load /tmp/fin2.bin --reps 8 [--from 1]               # ~1,5 min
```

Sorties brutes : `race_seed1.txt`, `race_seed2.txt`, `origin_N16384.txt`, `drift_sources_selection.txt` et
`drift_sources_temoin.txt`. Le résumé chiffré est dans `results.txt`. Le calcul complet tient en ~16 min sur 8 cœurs.

## 1. Fitness robuste et racing : la sélection bat-elle le hasard ?

### Protocole
Il est identique pour les deux bras, sélection (AG) et témoin (univers tirés au hasard).
- **Étape 0** : 48 univers × 12 générations, 2 graines par évaluation, N=1024, E=1024, avec arrêt précoce à 128
  époques sans naissance.
  - Score de classement : taux d'émergence lissé, (nem+0,1)/(nev+2). La fitness façonnée v1 sert à départager.
  - Pour l'AG, les élites (pop/4) reçoivent 2 graines neuves par génération (racing). Le témoin tire 48 nouveaux
    univers par génération.
- **Étape 1** : top 48 de l'archive, +4 graines neuves à N=1024.
- **Étape 2** : top 12, 4 graines à N=4096.
- **Finalistes** : top 6, classés au score N=4096, puis revalidés sur **12 graines jamais vues** à N=4096.
- Budget : 1344 simulations à N=1024 et 48 à N=4096 par bras, identique dans les deux bras. Chaque étape a son propre
  sel de graines.
- Tests : Fisher exact sur les essais poolés, permutation sur les taux par univers (20 000 tirages) et IC de Wilson.

### Résultats (2 courses indépendantes, graines maîtresses 1 et 2)

| course | bras | étape 0 émergences | revalidation (6 × 12 graines neuves) | Fisher unilatéral S > T | permutation par univers |
|---|---|---|---|---|---|
| 1 | sélection | 0/1152 | **0/72** (IC95 0–5,1 %) | p = 1 | p = 1 |
| 1 | témoin | 1/1152 | **3/72** (4,2 %, IC95 1,4–11,5 %) | | |
| 2 | sélection | 128/1152 | **33/72** (45,8 %) | **p = 0,00063** | **p = 0,013** |
| 2 | témoin | 5/1152 | **14/72** (19,4 %) | | |
| poolé | S vs T | | 33/144 vs 17/144 | p = 0,0095 (bilatéral 0,019) | — |

**Verdict.** Quand l'AG trouve un premier univers fertile (course 2, dès la génération 1), la sélection produit des
univers nettement plus fertiles que le hasard soumis au même racing : 46 % contre 19 % sur des graines jamais vues,
p = 6×10⁻⁴. Les six finalistes S ont tous entre 4/12 et 7/12. Le racing seul améliore déjà beaucoup le témoin :
19 % chez les finalistes, contre 0,4 % pour un univers aléatoire brut.

Le succès de l'AG n'est cependant **pas garanti**. En course 1, il n'a vu aucune émergence. Il a alors été guidé par
la seule fitness façonnée (naissances, part de pas productifs) et s'est enfermé dans un attracteur stérile
(`addr=ip`, L=32, mut 4e-3) : 0/72. Sur deux courses, on a un succès net et un échec net.

### Profil des finalistes
- **Sélection, course 2** : les 6 ont L=16 et kper=4 (une table par position mod 4). Deux sont en `abs` et quatre en
  `ip`. On trouve rcost 1–4, wcost 1–4, 512 à 1024 pas, mut 1e-3 (un seul à 0).
  - Le réplicateur dominant est minuscule et très creux : `35 00 00 00 00 33 00 33 33 00 33 00 00 00 00 35`.
  - Son squelette est READ_H / WRITE_H / H0_INC répétés et fermés par un LOOP, avec 10 à 11 octets essentiels sur 16.
  - C'est un attracteur **différent de la v1**, où l'on trouvait `b`, L=64 et mut 4e-3 : la v2 découvre que
    l'adressage `b` n'est pas nécessaire.
- **Témoin** : tous sont en `addr=b` (L=16 à 64, mut 2,4e-4 à 4e-3). Leurs scores sont de 0 à 5/12.

## 2. L'origine à N=16384

Même cadre que la v1. L'origine est la soupe `b` (L=64, 8192 pas, mut 2,4e-4). Les mêmes 8 graines neuves sont
utilisées pour tous les univers (comparaison appariée), avec N=16384 et E=2048.

| univers | émergences | époque d'émergence | Fisher > origine |
|---|---|---|---|
| origine (soupe b) | **0/8** (1 à 16 naissances isolées par graine) | — | — |
| S#0 (L16 abs) | **5/8** (+1 cristal) | 64–256 | p = 0,013 |
| S#1 (L16 abs) | 1/8 (+2 cristaux) | 192 | p = 0,5 |
| T#0 (L64 b) | **8/8** | 64–2048 | p = 7,8×10⁻⁵ |
| T#1 (L32 b) | **8/8** | 64–832 | p = 7,8×10⁻⁵ |

S'y ajoute l'origine à N=4096 : 0/12 dans chacune des deux courses.

**Réconciliation avec la soupe.** La soupe a émergé sur 2 graines sur 4 à N=16384, mais l'une à ≤256 époques et
l'autre vers 5000. Dans une fenêtre de 2048 époques, elle fait donc 1/4. Le cadre fait 0/8, et 0/8 contre 1/4 donne
Fisher p = 0,33 : **pas d'incompatibilité**. Le nombre de naissances isolées est aussi du même ordre (la soupe en
comptait ~10 en 16384 époques sur ses graines stériles). L'origine est dans un régime d'événement rare, environ
≤1 graine sur 8 à 1/4 par fenêtre de 2048 époques.

**À taille égale (N=16384), les univers trouvés la battent nettement.** S#0, T#0 et T#1 sont à 5/8 à 8/8 et
émergent en général avant l'époque 256. Seul S#1 n'est pas significativement meilleur. Effet notable :
**les finalistes S, optimisés à N=1024–4096, sont battus par deux finalistes T à 16384**. Les univers L=16
sélectionnés produisent davantage de cristaux et d'arrêts à grand N, signe d'un sur-ajustement de taille.

## 3. Migration : la vie suit-elle une physique qui dérive ?

### Protocole
- On ensemence 1/8 de la soupe (N=1024) avec le réplicateur dominant d'un univers fertile, puis on laisse
  s'établir pendant 512 époques (établissement 64/64 dans tous les cas).
- La dérive dure ensuite 4096 époques. Tous les K époques, un changement : 90 % une entrée de table (q, octet)
  → primitive tirée, 10 % un paramètre (rcost, wcost, pas, mut). L, kper et addr sont fixes. K ∈ {512, 128, 32, 8},
  soit 8, 32, 128 et 512 changements.
- Témoins :
  - **sans dérive** ;
  - **saut brutal** vers *exactement le même point d'arrivée* que chaque trajectoire de dérive. Le saut a lieu au
    début de la phase de dérive, avec la même soupe initiale.
- 4 sources (S#0 et S#1 en L16 `abs`, T#0 en L64 `b`, T#1 en L32 `b`) × 8 répliques.
- « Vivant » veut dire une fraction de copieurs vérifiés ≥ 0,25 à la fin, sous la physique finale.

### Résultats (poolés sur 4 sources × 8 répliques = 32 par case)

| K (changements) | dist. table moyenne | dérive vivants | saut vivants | Fisher dérive vs saut |
|---|---|---|---|---|
| sans dérive (0) | 0 | **31/32** | — | — |
| 512 (8) | ~6 | 25/32 | 20/32 | p = 0,27 |
| 128 (32) | ~28 | 22/32 | 21/32 | p = 1 |
| 32 (128) | ~100 | 9/32 | 11/32 | p = 0,79 |
| 8 (512) | ~200–360 | 1/32 | 3/32 | p = 0,61 |
| **tous K** | | **57/128** | **55/128** | **p = 0,90** |

Les valeurs de p par lot de sources sont dans les fichiers bruts : par exemple, dérive vs sans dérive à K=32 donne
p = 4×10⁻⁵ pour les sources S.

**Verdict.**
1. **Oui, la vie suit une petite dérive.** Jusqu'à ~30 entrées de table changées, soit ~11 % de la table, 70 à 80 %
   des populations survivent.
   - On observe de **l'adaptation véritable**. Dans 21 trajectoires de dérive, la population reste vivante alors
     que le réplicateur initial n'est **plus** copieur sous la physique finale.
   - Le dominant a alors réécrit ses octets essentiels. Pour S#1 à K=32, les essentiels deviennent
     `35 00 00 31 … 31 00 00 35` : de nouveaux octets prennent les rôles AND/SUB, et le LOOP est réencodé.
   - Pour T#0 (mut 4e-3), la population est une quasi-espèce : le dominant diffère de l'injecté sur ~63 octets sur 64,
     même sans dérive.
2. **La lenteur n'aide pas.** À point d'arrivée égal, la dérive lente et le saut brutal donnent la même survie
   (57/128 contre 55/128, p = 0,90). Ce qui tue, c'est la **distance cumulée** à la physique de départ, pas la
   vitesse du chemin. Au-delà de ~100 entrées changées, presque tout meurt dans les deux cas.
3. Le saut lointain de la v1 (0/47) est cohérent avec la ligne K=8 : un point d'arrivée quasi aléatoire, avec 1/32
   et 3/32 de survie.

## Pièges et limites (documentés honnêtement)
- **Fisher poolé** : il suppose les essais indépendants, alors que les 12 graines d'un même univers ne le sont pas.
  D'où le test de permutation par univers, plus conservateur (course 2 : p = 0,013 au lieu de 6×10⁻⁴).
- **AG bimodal** : sans émergence précoce, la fitness façonnée v1 est trompeuse (course 1). Une seule course aurait
  conclu à l'inverse. Il faudrait au moins 4 à 8 courses pour estimer la *probabilité* de succès de l'AG.
- **Racing** : le témoin en profite aussi (19 % contre 0,4 % brut). La comparaison mesure donc la valeur ajoutée de
  la *sélection* au-delà du *tri*.
- **Graines** : chaque étape a un sel distinct, et la revalidation utilise des graines jamais vues. Le classement des
  finalistes (top 6 parmi 12 au score N=4096) n'utilise pas les graines de revalidation.
- **Cristaux** (≤2 octets distincts) : exclus de l'émergence. 97 ont été rencontrés par l'AG en course 2, contre 5
  pour le témoin : l'AG va aussi vers des physiques qui cristallisent.
- **Origine** : E=2048 seulement. Une émergence tardive à la façon de la graine 0 de la soupe (~5000 époques) est
  hors fenêtre.
- **Dérive** :
  - La dérive n'atteint sa physique finale qu'à la dernière époque, alors que le saut a 4096 époques pour se
    réadapter. La comparaison est plutôt défavorable à la dérive pour les K courts, et pourtant les résultats sont égaux.
  - Des « morts » transitoires (copieurs 0,00 puis 1,00 au point de contrôle suivant) apparaissent dans les courbes.
    Elles sont probablement dues à un changement de paramètre (pas, coûts) qui casse temporairement la vérification
    de copie. Ce n'est pas vérifié en détail.
  - Un premier passage (4 répliques, sources S) avait un bug d'affichage de la distance de table « sans dérive »
    (992 au lieu de 0, car `end` n'était pas initialisé dans le mode 2). Le bug est corrigé avant les chiffres
    ci-dessus, et la survie n'était pas affectée.
- Les finalistes (`.bin`) ne sont pas versionnés. `race --seed 2 --save` les régénère de façon déterministe.

## Suite proposée
1. 6 à 8 courses de l'AG pour estimer P(succès). Ajouter un redémarrage quand aucune émergence n'est vue après
   g générations.
2. Sélection multi-N : un score à N=1024 **et** à N=16384, contre le sur-ajustement de taille des finalistes L=16.
3. Migration : mesurer le seuil de distance (survie en fonction des entrées changées) avec plus de répliques, et
   essayer une dérive *biaisée* (accepter un changement seulement si la population reste vivante) pour voir si une
   co-évolution physique/vie atteint des univers lointains.
