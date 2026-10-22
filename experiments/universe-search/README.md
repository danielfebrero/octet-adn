# Méta-évolution d'univers : faire évoluer la physique elle-même

Question de Dani : « des graines et des cycles pour l'interpréteur et l'univers, en croisant les populations ».
Au lieu d'évoluer des programmes dans **une** physique fixe (la soupe `b` relative), on évolue **la physique** :
la table qui dit ce que fait chaque octet, en fonction de sa position, et les paramètres du monde.
La fitness d'un univers est simple : **la vie y émerge-t-elle du bruit ?**

Code : `experiments/universe_search.c`. Le moteur cœur n'est pas modifié et `seeds/` n'est pas touché.
Le selftest tourne dans `make test`.

## 1. Le génome d'univers

Un univers est tiré d'une graine. Il contient :

| gène | valeurs |
|---|---|
| `tab[k][256]` | primitive élémentaire de chaque octet, éventuellement dépendante de la position (`octet`, `pos mod kper`) |
| `kper` | 1, 2 ou 4 (4 tables de 256 au plus) |
| `L` (taille d'un programme ; ruban = 2L) | 16, 32, 64 |
| adressage des immédiats | `abs` (k), `b` (B+k, comme la soupe), `ip` (position de l'instruction + k) |
| coût lecture / écriture | 1, 2, 4 pas |
| budget de pas par exécution | 256, 512, 1024, 2048 |
| mutation de fond | 0, 1e-4, 2.4e-4, 1e-3, 4e-3 par octet et par époque |

`N`, le nombre de programmes de la soupe, est **fixe** (1024) et n'est pas évolué. Sinon, la sélection
« achèterait » simplement du calcul, puisque l'émergence est d'autant plus probable que la soupe est grande.
C'est un paramètre de budget, pas de physique.

### Catalogue : 28 primitives élémentaires, garde-fou anti-triche

`NOP HALT INC_A DEC_A INC_B DEC_B CLR_A CLR_B MOV_BA MOV_AB ADD SUB XOR LOADI·k AND·k CLRI·k LOAD·k STORE·k
JMP·k JZ·k JNZ·k LOOP READ_H WRITE_H H0_INC H0_DEC H1_INC H1_DEC` (deux têtes à 0, comme BFF).

Il n'y a **aucune** copie de bloc, duplication ou SPLIT. Seules `STORE` et `WRITE_H` écrivent, **un octet**
chacune. Un réplicateur doit donc construire sa boucle de copie octet par octet. Le selftest vérifie :
1. statiquement, que le catalogue compte 28 primitives, 2 écrivains et aucun nom de type bloc ;
2. dynamiquement, sur 1800 univers aléatoires, croisés, mutés ou saturés d'écrivains, que la VM en mode contrôle
   ne change jamais plus d'1 octet par instruction, et 0 pour une primitive non écrivaine
   (résultat : 0 violation) ;
3. que l'univers **origine** (table = opcodes 0x00–0x1F de la soupe, adressage `b`, L 64) reproduit `soup_exec`
   à l'octet près sur 3000 rubans. Seule déviation documentée : `NEURAL 0x13`, qui n'a pas d'équivalent
   élémentaire et est mappée sur `CLRI`. Les divergences où un 0x13 apparaît sont excusées et comptées ;
   il y en a 1 sur 3000. Le copieur fait main est reconnu dans les deux, et le bruit n'est pas un copieur ;
4. qu'une injection du copieur fait main émerge, que le run est déterministe et que `no-store` donne 0 naissance.

## 2. Fitness d'un univers (même critère que la soupe)

- On part d'une soupe de bruit de N=1024 programmes, sur E=1024 époques. Les paires sont tirées au hasard,
  le ruban 2L est exécuté depuis ip 0, puis recoupé.
- **Naissance** : une moitié est réécrite (≥ L/8 octets) en copie fidèle (≥ L−L/8) de l'autre moitié.
- **Copieur vérifié** : copie fidèle sur 3 partenaires de bruit sur 4, et l'enfant copie aussi.
- **Émergence** : la fraction de copieurs dans un échantillon de 64 dépasse 0.25 à un checkpoint (toutes les
  64 époques). Les octets essentiels sont mesurés par ablation (4 substitutions ; essentiel si au moins 3 cassent).
- **Cristaux exclus** : un réplicateur homopolymère (≤ 2 valeurs d'octet distinctes, par ex. `58 58 58 …`)
  n'est **pas** compté comme émergence. Voir § 5.
- Tri précoce : arrêt à 128 époques s'il n'y a eu aucune naissance.
- Score :
  - si émergence : `3 + frac_copieurs + (1 − t_émergence/E)` ;
  - sinon : `[1.5 + 0.5·log(naissances par copieur vérifié)]` (seulement s'il y en a, et hors cristal)
    `+ 0.5·log(naissances)/log(N) + 0.4·copie_partielle`.

Boucle externe : population de 48 sur 20 générations. Le quart élite est ré-évalué sur des graines neuves et moyenné.
Le reste est produit pour moitié par croisement 2 points des tables plus mutation, pour moitié par mutation seule
(1 à 8 entrées de table, plus un paramètre avec p=0.3).
**Témoin obligatoire** : 960 univers aléatoires frais, soit le même nombre d'évaluations, sans sélection.
**Revalidation** : les 8 meilleurs univers fertiles de chaque bras sont rejoués sur 4 graines fraîches.

## 3. Résultats (3 graines, N=1024, E=1024, 8 cœurs, ~8 min au total)

### Taux d'émergence : sélection vs hasard

| | sélection | témoin aléatoire | rapport |
|---|---|---|---|
| évaluations avec émergence (brut, 3×960) | **82/2880 = 2.85 %** (31, 25, 26) | **16/2880 = 0.56 %** (7, 3, 6) | **×5.1** |
| générations 10–19 seulement | 58/1440 = **4.0 %** | — | **×7** |
| univers distincts fertiles au moins une fois | 65 (25, 21, 19) | 16 (7, 3, 6) | ×4 |
| cristaux homopolymères | 0 | 4 | |
| **revalidation sur graines fraîches** | **8/96 = 8.3 %** | **2/64 = 3.1 %** | ×2.7, **p ≈ 0.16** (Fisher unilatéral) |
| univers origine (soupe `b`), 8192 pas / 2048 pas | 0/24 / 0/24 | | |

Courbe de fertilité (émergences sur 48 par génération, sélection) :
```
graine 1 : 0 1 0 3 1 1 1 4 1 1 0 0 1 6 5 1 1 2 1 1
graine 2 : 0 0 0 2 3 0 0 0 1 0 2 3 3 0 4 0 1 3 2 1
graine 3 : 1 0 0 0 1 0 2 0 1 0 2 1 0 1 3 3 2 5 1 3
témoin   : 0–2 par génération, moyenne 0.27
```
La fitness moyenne de la population sélectionnée monte de 0.12–0.19 à 0.7–2.5. Les arrêts précoces passent
de ~38/48 à ~5/48. Le témoin reste plat, autour de 0.1–0.3.

**Lecture honnête.** La sélection **bat le hasard** pour *trouver des régions fertiles* de l'espace des
physiques : elle trouve 5 à 7 fois plus d'émergences par évaluation et 4 fois plus d'univers fertiles distincts.
Mais chaque univers trouvé reste **faiblement fertile**. Rejoué sur des graines neuves, il n'émerge que
~8 % du temps, contre ~3 % pour un univers aléatoire déjà vu fertile une fois. Cet écart n'est pas significatif
avec 160 essais. Une partie du gain brut vient de ce que l'élite rejoue des univers voisins, donc les essais
ne sont pas indépendants. Les populations convergent vers une ou deux lignées de tables quasi identiques.

**Origine battue à cette échelle.** La physique de la soupe `b` relative fait 0/48 à N=1024, ce qui est cohérent
avec son README, où il fallait N bien plus grand. Le hasard pur trouve des univers fertiles à N=1024 dans ~0.6 %
des tirages, et la sélection dans ~4 %. La physique « faite main » n'est donc pas un optimum.

### Version 1 de la fitness (échec instructif, conservé)

La première fitness récompensait les naissances et la copie partielle sans exiger que le parent soit un
copieur vérifié. La sélection s'y est fait **piéger**. La fitness moyenne montait à 1.8 en optimisant des
« naissances » de remplissage, sans réplicateur. Résultat sur la graine 1 : sélection 7/960 (0.7 %) contre
témoin 9/960 (0.9 %) ; revalidation 1/24 contre 6/32. La sélection était **pire** que le hasard.
Le correctif est le terme « naissance par un copieur vérifié », plus l'exclusion des cristaux.
C'est le résultat de la v2 ci-dessus.

### Profil des univers fertiles (9 revalidés, sélection)

- **adressage `b` : 9/9** (relatif à B, comme la soupe ; `abs` et `ip` jamais) ;
- **L = 64 : 7/9** ;
- **mutation de fond 4e-3 (le maximum) : 8/9**. Un fond mutagène aide la nucléation ;
- coût d'écriture 1 : majoritaire. Budget 1024–2048 pas ;
- `kper` 1/2/4 : 1/3/5, donc une table dépendante de la position est favorisée ;
- part des primitives dans la table ≈ uniforme, avec un léger enrichissement en `JZ` (0.052) et `LOOP` (0.049)
  contre 0.036. Ce qui compte, c'est **quels** octets portent la boucle, pas la composition globale.

Réplicateurs : 6 à 16 octets essentiels sur 64. Les primitives essentielles les plus fréquentes sur les 3 graines
sont `WRITE_H` (17), `LOOP` (9), `LOAD` (9), `H1_DEC` (7), `DEC_B` (6), `STORE` (4).
Deux motifs de copie reviennent :
- **tête de lecture/écriture** : `READ_H … H1_DEC WRITE_H … LOOP` (BFF-like) ;
- **adressage relatif** : `DEC_B LOAD … STORE … LOOP`. C'est le copieur de la soupe, redécouvert dans une
  autre table, par exemple `DEC_B/LOAD/XOR/STORE/…/LOOP` sur 8 octets (run v1) ou `DEC_B · WRITE_H · LOAD · JZ · H1_DEC · WRITE_H · LOOP` (graine 2, S#2).

Exemple (graine 1, S#1, 8 essentiels) :
`H1_DEC · LOAD · CLRI · WRITE_H · DEC_B · LOADI · INC_A · LOOP`.

### Cristaux

Sans garde-fou, le hasard produit des réplicateurs **homopolymères**. Par exemple `0x58 × 64` avec kper=4 :
selon sa position, le même octet vaut `H1_DEC`, `WRITE_H` ou `READ_H`, ce qui forme une boucle tête-à-tête
qui recopie l'octet jusqu'au bout du ruban. Autre exemple : `0xee × 16` en adressage `abs`. C'est légal,
puisque ce sont des primitives élémentaires, mais **sans information**, comme un cristal qui croît.
Ils sont comptés à part (4 dans le témoin, 0 en sélection v2) et exclus de la fitness.

### Migration : croiser les populations entre univers

32 copies du réplicateur dominant d'un univers source sont injectées dans une soupe de bruit d'un univers cible,
pendant 512 époques. Cibles : les fertiles, l'origine et deux aléatoires.

| | résultat |
|---|---|
| auto-migration (témoin positif) | immigré copieur 9/9, **invasion 2/9** |
| migrations croisées | **invasion 0/47**, immigré copieur direct 2/47 (seulement entre univers frères d'une même lignée) |
| vers l'origine ou les univers aléatoires | 0 |

**La vie ne survit pas à un changement de physique.** Un réplicateur est une boucle écrite *dans* une table
précise. Dans une autre table, les mêmes octets sont un autre programme. L'invasion échoue même en
auto-migration dans 7 cas sur 9, parce que ces univers sont faiblement fertiles : 32 copies dans 1024 programmes,
à fidélité médiocre, s'éteignent souvent. Le test a donc peu de puissance, mais les 2 seuls succès de transfert
direct concernent des univers à 1–8 entrées de table près.

## 4. Verdict

- **Oui, la sélection d'univers bat le hasard pour trouver des physiques fertiles** : ×5 en brut, ×7 en fin
  de run. Sur graines fraîches, le gain se réduit à ×2.7 (8.3 % contre 3.1 %), ce qui n'est **pas significatif**
  (p≈0.16).
- Les physiques fertiles se ressemblent : adressage relatif `b`, programmes longs, fond mutagène fort,
  écriture bon marché. La soupe d'origine avait deviné juste sur l'adressage `b`.
- La physique d'origine est **battue** à N=1024 (0/48), aussi bien par le hasard que par la sélection.
- La vie n'est **pas transférable** entre physiques (0/47).
- Le piège v1 montre qu'une fitness « naissances » mal définie est trompeuse : la sélection optimise du
  remplissage. Il faut exiger un copieur vérifié et exclure les cristaux.

## 5. Reproduire

```
make experiments/universe_search
./experiments/universe_search selftest
./experiments/universe_search run --seed 1 --pop 48 --gens 20 --N 1024 --epochs 1024 --val 4   # ~2.5 min / 8 cœurs
# options : --fixL 16|32|64  --threads 8  --migrate 0|1  --early 128
```
Sorties complètes : `results.txt` (graines 1, 2, 3).

## 6. Prochaine étape

Rendre les univers *robustement* fertiles, et pas seulement chanceux :
1. évaluer chaque univers sur 2 à 4 graines (fitness = taux d'émergence) avec une réduction successive
   (successive halving), et monter N à 4096 pour les finalistes ;
2. re-tester l'origine à N=16384 pour situer son seuil ;
3. faire de la migration *dans* une lignée, entre univers voisins, pour voir si la vie suit une physique qui
   dérive lentement : l'évolution conjointe de la vie et de la physique.

## 7. Suite : v2 (voir `v2/README.md` et `v2/results.txt`)
- **Fitness robuste + racing** : la course 2 donne sélection 33/72 contre témoin 14/72 sur des graines neuves à
  N=4096 (Fisher p = 6×10⁻⁴, permutation p = 0,013). La course 1, où l'AG n'a vu aucune émergence, donne 0/72 contre
  3/72. L'AG est bimodal. Poolé : p = 0,0095.
- **Origine à N=16384** : 0/8 en 2048 époques, ce qui est compatible avec la soupe (1/4 dans la même fenêtre,
  p = 0,33). Les univers trouvés la battent à taille égale (5/8, 8/8, 8/8).
- **Dérive lente** : la vie suit une petite dérive (≤ ~30 entrées de table), avec de l'adaptation réelle des
  réplicateurs. Mais elle ne fait pas mieux qu'un saut brutal vers le même point d'arrivée (57/128 contre 55/128,
  p = 0,90) : c'est la distance qui compte, pas la vitesse.
