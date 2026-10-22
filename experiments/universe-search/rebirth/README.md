# Renaissance : la vie qui revient peut-elle durer plus longtemps ? (v2, partie 4)

Question de Dani : « la vie qui renaît après la mort peut-elle cette fois être plus persistante ? Je veux suivre
cet univers qui donne la vie et l'enlève. » L'univers en question est S#0 rép 5, le « survivant » de la dérive K=8
(cf. `../k8-survivor/`).

**Réponse courte : non, pas de façon durable.** Le renaissant est un peu plus robuste que le premier-né, mais
l'écart n'est pas significatif. Personne ne survit à une dérive continue. Si on suit l'univers longtemps, les vies
raccourcissent puis la physique devient stérile : la dernière vie s'éteint à l'époque 14272 sur 33280. La
persistance est une propriété de la physique, pas du réplicateur. Ce qui rend une physique fertile, ce n'est pas sa
composition globale : c'est la survie de lois ancestrales qui fonctionnent.

## Outil

`experiments/rebirth.c` inclut `k8_survivor.c` sans son `main` (garde `K8_NO_MAIN`, +2 lignes, comportement
inchangé). Il est câblé dans le `Makefile` et son selftest fait partie de `make test`. Les deux fonctions centrales :
- `life_run` est une copie de `drift_run` (mêmes tirages, dans le même ordre). Je lui ai ajouté :
  - la bifurcation de la graine de dérive ;
  - l'effacement de la soupe à chaque mort ;
  - le journal des générations.
- `phys_replay` reconstruit la physique à n'importe quelle époque de dérive sans simuler la soupe. C'est possible
  parce que le générateur de dérive est indépendant de la soupe.

Le selftest vérifie quatre choses :
- `life_run` == `drift_run` ;
- la fin de `phys_replay` == la physique finale de `drift_run` ;
- une bifurcation après la fin est neutre, une bifurcation avant la fin a un effet ;
- les statistiques de rang (Spearman, Kendall stratifié, AUC) donnent les valeurs attendues.

    ./experiments/universe_search2 race --seed 2 --save /tmp/k8_fin2.bin          # finalistes v2 (déterministe, ~4 min)
    ./experiments/rebirth pool     --load /tmp/k8_fin2.bin --save /tmp/pool.bin   # 132 physiques × bruit pur  (3m38)
    ./experiments/rebirth persist  --load /tmp/k8_fin2.bin --pool /tmp/pool.bin   # Q1                         (2m24)
    ./experiments/rebirth cycles   --load /tmp/k8_fin2.bin                        # Q2                         (1m19)
    ./experiments/rebirth features --load /tmp/k8_fin2.bin --pool /tmp/pool.bin   # Q3                         (1 s)

Sorties brutes : `pool.txt`, `persist.txt`, `cycles.txt`, `features.txt`. Calcul total : environ 7,5 min sur 8 fils.

Définitions :
- **Mort** : 0 copieur vérifié dans la population **entière** (1024 programmes) à un point de contrôle. Les points
  de contrôle tombent toutes les 64 époques.
- **Continuité stricte** : la lignée ne passe jamais par une mort.

## Q1 : le renaissant est-il plus persistant que le premier ?

Même protocole pour 37 réplicateurs, chacun testé dans sa physique de naissance :
- `rob@n` : part de 200 tables où n entrées ont été re-tirées et où le génome copie encore.
- `mrob` : part des mutants ponctuels qui copient encore.
- Une nouvelle dérive K=8 : 8 graines, 4096 époques, continuité stricte.
- Survie sans dérive à 4 taux de mutation.

| | rob@32 | rob@352 | mrob | vie sous dérive (ép.) | survie stricte |
|---|---|---|---|---|---|
| premier injecté S#0 `3500ae…ae0035` | 0.750 | 0.075 | 0.118 | 480 | 0/8 |
| renaissant survivant `3500…f1…0035` | 0.815 | 0.145 | 0.125 | 856 | 0/8 |
| 8 renaissances indépendantes (bruit, physique S#0 r5 finale) | 0.836 | 0.151 | 0.248 | 1239 | 3/64 |
| premiers-nés, physique de départ (n=14) | 0.619 | 0.065 | 0.532 | 813 | 1/112 |
| premiers-nés, physique de fin de dérive (n=13) | 0.597 | 0.048 | 0.667 | 661 | 0/104 |

Tests de permutation bilatéraux, unité = réplicateur.

Renaissants S#0 (n=11) contre premiers-nés S#0 (n=4) :

| mesure | renaissants | premiers-nés | p |
|---|---|---|---|
| rob@352 | 0.149 | 0.111 | 0.074 |
| vie | 1095 | 778 | 0.19 |
| mrob | – | – | 0.90 |

Tendance favorable au renaissant, mais aucun test n'est significatif.

Contrôle « né en physique de fin de dérive » (n=22) contre « né en physique de départ » (n=15) : aucune différence
(rob@352 p=0.34, vie p=0.51, survie à 4e-3 p=1.0). En se restreignant aux sources T, mrob donne 0.796 contre 0.726
(p=0.032). C'est un test parmi 15, il ne résiste donc pas à une correction pour tests multiples.

Ce qui différencie vraiment les réplicateurs, c'est la **famille de physique** :
- Les physiques S (L=16, mutation native ≤1e-3) donnent des génomes robustes aux changements de loi
  (rob@352 ≈0.1-0.2) mais fragiles aux mutations : ils meurent à 4e-3.
- Les physiques T (L=32/64, mutation native 4e-3) donnent l'inverse : rob@352 ≈0, survie à 1.6e-2.

Sur l'ensemble, la survie stricte à une nouvelle dérive K=8 est de **4/296** couples (réplicateur, graine). Le
compte des renaissances E5 (3/72 contre 1/224, Fisher unilatéral ≈0.046) est un regroupement fait a posteriori,
qui de plus traite les graines comme indépendantes. L'effet est minuscule.

## Q2 : suivre l'univers qui donne la vie et l'enlève

Protocole :
- S#0 rép 5, graine `mix(7,5)`, 512 époques d'établissement puis 32768 époques de dérive continue.
- Le run 0 prolonge la trajectoire d'origine. Le selftest vérifie que sa physique à dé=4096 est identique à celle
  de k8.
- Les runs 1 à 7 bifurquent la graine de dérive après dé=4096.
- Chaque run est joué deux fois sur la **même suite de physiques** : soupe conservée à la mort, ou soupe remplacée
  par du bruit.
- Naissance : au moins 51 copieurs vérifiés.

Résultats :
- **Morts et renaissances, soupe conservée :** 3 à 10 générations par run (33 vies achevées). La vie n'est
  présente que 11 à 23 % du temps (moyenne 13,8 %).
- **Toutes les vies finissent par mourir :** il n'y a aucune vie après l'époque 11648 dans les runs avec soupe
  conservée, ni après 14272 avec effacement.
- **L'univers devient stérile :** émergence depuis le bruit sous la physique du run 0 à 4/4 à dé=2048, 1/4 à
  4096 et à 6144, puis 0/4 à chaque point de dé=8192 à 32768.
- **Pas de progrès d'une génération à l'autre :**
  - Kendall stratifié τ=-0.24, P(hausse)=0.92, P(baisse)=0.10. Spearman intra-run moyen 0.10.
  - La 1re génération (injectée) vit 1664 époques, les renaissances 932 en moyenne (p=0.049).
  - Avec effacement : τ=-0.39 (P(baisse)=0.077), 1664 contre 1147 (p=0.051).
- **Héritage, comparaison appariée conservation / effacement :**

  | | soupe conservée | effacement | p |
  |---|---|---|---|
  | attente avant renaissance | 545 ép. | 2409 ép. | 0.0001 |
  | H au dernier dominant de la vie précédente | 6.6/16 | 10.1/16 | 0.006 |
  | renaissances proches (H≤4) | 10/25 | 0/14 | |
  | part du temps vivant | 13.8 % | 11.0 % | 0.21 |

  La soupe conservée garde des fragments de la famille, mais presque jamais de copie intacte : reliques H≤2 = 0 à
  la plupart des morts. Ces fragments accélèrent la renaissance sans allonger la vie.
- **Dégénérescence tardive :** les dernières renaissances avec effacement sont des homopolymères copieurs
  (`d9d9…`, `5050…`, `5d5d…`), des réplicateurs triviaux d'une seule instruction.

## Q3 : qu'est-ce qui rend un univers fertile, et létal ?

Échantillon : 132 physiques (4 bases, 32 fins de dérive, 96 points intermédiaires), dont 40 fertiles.
- **La composition ne prédit rien.** J'ai testé 41 caractéristiques : 28 parts de primitives, LOOP, READ/WRITE,
  LOAD+STORE, adressage, coûts, steps, mutation. La meilleure AUC est 0.644 (taux de mutation), p familial = 0.22.
  Une règle à seuil validée en laissant une source de côté donne une exactitude équilibrée de 0.549, soit le hasard.
- **La survie des lois ancestrales fonctionnelles prédit la fertilité :**
  - Le réplicateur injecté de la source copie encore dans 10/40 physiques fertiles contre 4/92 stériles
    (Fisher p=0.001).
  - La part des réplicateurs nés ailleurs qui copient sous la physique donne AUC 0.71 (0.164 contre 0.034).
- **La fertilité n'est pas monotone sur 4096 époques** (de0 4/4, puis 11/32, 7/32, 8/32, 10/32). Sur la longue
  dérive du run 0, elle tombe à 0 dès dé≈8000.
- Lecture : une physique devient létale quand la dérive a réécrit assez de lois pour qu'aucun réplicateur
  existant ou proche ne s'y exécute. Une physique est fertile tant que des lois « qui ont déjà porté la vie »
  survivent.

## Verdict

La vie qui renaît n'est **pas** plus persistante que la première, du moins pas d'une façon qui se mesure :
- tendance non significative en robustesse (p=0.07) et en durée de vie (p=0.19) ;
- survie stricte ≈0 pour tous ;
- au fil des cycles, les vies raccourcissent (1664 → 932, p=0.049) puis l'univers devient stérile.

L'univers « donne et reprend » tant que sa physique garde des lois ancestrales. La soupe transmet de la vitesse de
renaissance, pas de la durée de vie. La persistance appartient à la physique, pas au génome.

## Expérience suivante

**Test causal de la conservation des lois.** On rejoue la même longue dérive (S#0 r5, 32768 époques, 8 graines)
avec trois bras :
- (i) les entrées de table essentielles au dominant du moment sont **protégées** de la dérive. On les ré-évalue à
  chaque point de contrôle, ce qui revient à une vie qui « tient » ses lois ;
- (ii) on protège le même nombre d'entrées tirées au hasard ;
- (iii) aucune protection.

Mesures : temps vivant, nombre de morts, fertilité depuis le bruit en fin de run, et distance parcourue par la
physique. Si la fertilité est la conservation des lois fonctionnelles, (i) doit donner une lignée continue et (ii)
ne doit pas faire mieux que (iii). C'est la première forme de **co-évolution vie-physique** où la vie agit sur ses
lois.
