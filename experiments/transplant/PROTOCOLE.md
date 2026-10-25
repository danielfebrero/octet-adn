# Transplant — faire se rencontrer les vivants de différents mondes

Préenregistré le 25 octobre 2026, avant la série. Outil `experiments/transplant.c` (commit 61a1a6a, correctif
de fourche ensuite). Validé par Daniel avec les dix suites. **Aucun monde vivant n'est modifié** : leurs
`state.ck` sont ouverts en lecture seule, chaque expérience vit dans un répertoire neuf sous
`experiments/transplant-runs/`. Les mondes ne sont jamais arrêtés pour être lus.

## Ce que la lecture des mondes impose

`transplant info` sur les mondes vivants : deux génétiques incompatibles. **Bandes de 64 octets**, copiées par
la soupe (`repro=1`) : monde, forces, causes, les témoins, et les quatre mondes à 32 000. **Division** à
longueur libre (`repro=0`) : capture, sénescence, parenté, rencontres. Les rencontres se font donc **dans une
même famille**. Les règles des mondes rencontres (échange, sénescence, capture, parenté) vivent dans une
extension que le terrarium de base n'a pas : chez eux, le jardin n'applique que la physique de base, commune
aux quatre. Les mondes à `.so` (tronc, lecture) et à 32 000 (lents) ne sont pas utilisés.

## A. Jardin réciproque (adaptation locale) — famille bandes

Mondes : `vie-monde` (forces 0), `vie-forces` (forces 1, espèce ATTACK), `vie-causes` (forces 2, causes).
Jardin = un îlot neuf aux règles de l'hôte (ses Params ; K=1, migration et expansion coupées pour lire
l'origine), 64 fondateurs, moitié génomes vivants de A, moitié de B, tirés au hasard ; 20 000 ticks.
Pour chaque paire (X, Y) : hôte X et hôte Y, mêmes graines. Graines 1001..1010.

- s_X(h) = part des vivants d'origine X à la fin, sous les règles de l'hôte h.
- **Avantage du pays** LA = s_X(X) − s_X(Y) (= s_X(X) + s_Y(Y) − 1 sans origine inconnue).
- **Critère par paire** : LA > 0 dans au moins 9 graines sur 10 (binomial unilatéral p ≤ 0,011 ; Bonferroni
  sur 3 paires, seuil 0,0167). Une égalité compte comme un échec. Tenu → adaptation locale.
- Témoins de dérive : X contre X sous l'hôte X (même monde des deux côtés), distribution de s rapportée.

## B. Tournoi — famille division

Capture, sénescence, parenté, rencontres-claude ont les mêmes Params : un seul hôte (sénescence), les 6 paires,
graines 1001..1010, 20 000 ticks. Part finale de chaque origine. **Descriptif, sans hypothèse**.

## C. Téléportation — fourches d'un monde vivant

Sources : `vie-monde` et `vie-causes`, fourchées en mémoire à leur dernier checkpoint, puis 10 000 ticks avec
le moteur de base. Bras, graines 1001..1010 :

| bras | geste au tick 0 |
|---|---|
| témoin | aucun |
| retrait | chaque créature retirée avec probabilité 0,5 (énergie mise à 0) |
| arrivée étrangère | 10 % des résidents remplacés sur place par des génomes vivants de l'autre monde |
| arrivée même monde | 10 % remplacés par des génomes vivants du même monde (même geste, sans étrangeté) |

- Retrait : population finale rapportée à celle du témoin de même graine ; descriptif.
- **Critère (par source)** : part finale des arrivants « même monde » − part finale des arrivants étrangers > 0
  dans au moins 9 graines sur 10 (Bonferroni sur 2 sources). Tenu → les résidents résistent davantage à
  l'étranger qu'aux leurs (le pendant de l'adaptation locale à l'intérieur d'un monde).

## Limites annoncées

Le moteur de base continue les mondes : pour monde/forces/causes c'est leur physique ; pour la famille division
il manque l'extension rencontres. Les fourches partent d'un instant ; le témoin partage cet instant. L'origine
se lit par la filiation ; la migration est coupée dans les jardins, laissée dans les fourches (K îles) où une
origine « inconnue » est comptée à part. Aucune conclusion sur l'intelligence : on mesure qui persiste où.

## Pilotes déclarés (non utilisés comme mesure)

Graine 1, 20 000 ticks : sous les règles de `vie-monde`, monde contre causes → causes éteint avant 2 000 ticks,
monde ≈ 950 vivants ; sous les règles de sénescence, sénescence 123 contre capture 37, démographie figée dès
4 000 ticks. Fourche témoin de `vie-monde`, 600 ticks : population stable. Fumée du pipeline d'analyse sur
800 ticks (graines 1001–1002, capture contre sénescence) : chiffres jetés, la série repart de zéro.
Correctif avant série : la fourche chargeait dans une copie libérée (les îles pointaient vers les paramètres
de l'original) ; elle charge maintenant directement dans la structure de l'expérience.
