# Attention

Le monde répond à un choix seulement s'il a vu le processus qui y mène.

Le processus est le nombre d'opcodes exécutés avant le premier acte
(`STEP`, `TURN`, `EAT`, `SPLIT`, `EMIT`, `ATTACK`, `YIELD`). Il dépose
cette profondeur, plafonnée à 15, comme chaleur sur la case de départ.
La chaleur refroidit de 1 à chaque tick.

Un acte sur une case encore chaude en dépense 1 :

- `STEP` pose 1 nourriture à l'arrivée
- `TURN` pousse 1 chaleur sur la case regardée
- `EAT` pose 1 nourriture sur la case derrière
- `EMIT` ajoute 1 chaleur aux quatre voisins
- `SPLIT` offre 2 à la case où naît l'enfant
- `ATTACK` divise par 2 la chaleur qui reste
- `YIELD` écrit seulement l'écho

Un réflexe, acte sans opcode avant lui, sur un sol froid ne reçoit rien.
Une hésitation (processus sans acte) laisse la chaleur et efface l'écho.

Le quartet haut du stimulus, sur une case chaude, devient l'écho du
dernier choix, sinon la chaleur elle-même. Le quartet bas (signal social
ou environnement) ne change pas. Une zone de prédateur garde le quartet
haut à 15.

`--attention 0` ne pose pas le relevé d'opcodes et ne tire aucun nombre
de plus. L'écriture des checkpoints est `OADNTER7`. Un `OADNTER6` se
relit avec attention à 0. Le binaire est `experiments/terrarium_attn` :
les cinq mondes déjà lancés gardent leur binaire.
