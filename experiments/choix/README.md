# Choix : protocole M1 puis M2

Le [plan arrêté](PLAN-VALIDE.md) est transcrit depuis la session du débat du
25 octobre 2026, avant toute mesure de cette campagne. Ce banc conserve la VM
`src/vm.c`, son ISA et son RNG, compilés directement dans un ELF neuf. Il fournit
un **milieu d'épisodes**, sans grille vivante ni interaction. Aucun objet, ELF,
checkpoint ou processus des 18 mondes n'est modifié.

## Préenregistrement v1, avant campagne

- 32 graines, 0..31 ; 256 épisodes par vie et par bras. Aucun épisode omis du
  dénominateur. L'unité des IC est la graine ; IC de Student, 31 ddl, 2,039513.
- Candidat déclaré : `seeds/general.adn`, existant avant le débat, sans édition,
  sans reproduction et sans sélection dans cette campagne. Il n'a pas été
  optimisé pour ce nouveau milieu. Un négatif sur lui ne réfute pas la capacité
  de toute population octet-adn. Le nom et le SHA256 sont figés dans le manifeste.
- Les politiques écrites T8 et oracle sont des **contrôles**, jamais des candidats
  évolués. T8 = un registre A plus une règle fixe ; M2 ajoute B et XOR. L'oracle
  informé utilise exactement le même bytecode et les mêmes coûts que T8 : ce
  doublon explicite montre qu'un contrôle fixe explique un succès éventuel.
- M1 : H indépendant Bernoulli(1/2), C correct avec probabilité 3/4. Lecture
  seulement au tick 0 ; ticks 1..4 neutres ; premier EMIT au tick 5 engage la
  route A&1 ; récompense +20 ou 0 au tick 9. EMIT hors décision ne choisit rien.
- Consultation 1 (bas) ou 8 (haut), une seule facture par fenêtre d'indice,
  même si plusieurs SENSE_STIM/NEURAL sont exécutés. Masque 0 avant/après.
  Le hook intervient avant le capteur réellement décodé. Aucun immédiat n'est
  pris pour un capteur. SENSE_HIST renvoie toujours 0 dans ce milieu.
- Espérance M1 avant métabolisme : aveugle 10, informé bas 14, informé haut 7.
  Au coût haut le contrôle rationnel ne consulte pas et reçoit en moyenne 10.
  L'absence de consultation n'est pas une omission de décision.
- Dotation initiale fixée à 100000, hors campagne : suffit même sans gain aux
  256 épisodes, chaque vie demandant au plus 2816 ticks + 4096 de consultation.
  Métabolisme = 1 par tick vivant, séparé du gain net (récompense − consultation).
  Aucun rechargement entre épisodes ; un décès reste compté dans tous les suivants.
- Les actions spatiales, manger, attaquer et diviser sont sans effet dans ce
  banc ; nourriture, voisins et historique neutres. Aucun coût physique variable
  ni support de rétention spatial. A/B, IP, ADN soma et r restent disponibles.
  `regs=1`, budget 16 opérations/tick, même configuration pour tous.
- Ablation après dernière observation : ADN restauré, A/B/IP/r et tous les états
  calculatoires effacés, âge/énergie conservés. Cette remise à zéro désynchronise
  aussi le programme ; elle n'est pas interprétée comme preuve unique.
- Identité d'individu/lignée = (task, seed, split, arm, 1), génome initial hashé
  dans chaque ligne. Clone indépendant entre bras, état conservé pendant la vie.
  Pas de mutation ni reproduction pendant évaluation.
- RNG adressé par tâche, graine, épisode, partition et flux. H/C indépendants du
  nombre de consultations ou d'opcodes. Brouillage indépendant du milieu.
- Contrôles sans indice : A, B, alternance et aléatoire. Comparaison conservatrice
  au maximum de leurs gains **par graine** (plus favorable que leur espérance).
  Lookup M1 ne dispose que de K, indépendant de H : repli aveugle 0.
- Critères M1 du candidat : réussite ≥0,65 ; gain net supérieur au meilleur
  aveugle, borne basse IC apparié >0 ; chutes sous brouillage ET ablation ≥0,10
  avec bornes basses >0 ; brouillage compatible avec 0,50 (IC la contenant).
- Validation du dispositif : oracle/T8 M1 dans ±0,04 de 0,75 (net dans ±0,8 de
  14), aveugles dans ±0,04 de 0,50 ; coûts exacts, aucune omission/décès des
  contrôles. Le brouillage du contrôle doit avoir IC contenant 0,50 et largeur
  ≤0,10. Aucun nouveau tirage si ce critère statistique échoue.

## M2, seulement après validation du dispositif M1

Symboles u,v dans 0..19. H = (u XOR v)&1. u au tick 0, v au tick 1 ; quatre
ticks neutres 2..5, décision au tick 6, paiement au tick 10. Chaque lecture coûte
1, séparément. Ni u ni v seuls ne prédisent H : moitié des sorties dans chaque
ligne et chaque colonne, dans les deux partitions.

Les 400 couples sont écrits dans `partition.tsv` **avant** les essais. Les 80
couples où (u+v)%5=0 sont réservés : jamais utilisés dans la partition vue, ni
pour sélectionner/modifier le candidat. Les 320 autres sont vus. Des clones de
naissance distincts évaluent les deux partitions. Il n'y a pas d'apprentissage
du candidat dans ce premier passage. « Inédit » signifie ici non exposé, pas
une preuve d'apprentissage préalable.

Lookup témoin : table explicitement construite sur les 320 couples vus, repli
0 sur les réservés. Deux lectures sont exécutées et facturées dans la VM avant
la décision prescrite. C'est un plafond pour une table mémorisant exactement les
tuples, pas un agent dont on revendique la découverte. Son calcul externe
prescrit des actions de contrôle ; il ne fournit aucune information au candidat.
T8 compose via XOR dans la VM, atteint 100 % par construction ; ce succès
ne constitue pas une émergence. Deux brouillages distincts neutralisent u ou v.

Critères candidat sur réservé : réussite ≥0,70, borne basse IC >0,50, écart
absolu vu/réservé ≤0,10 ; chute ≥0,10 sous chacun des deux brouillages, borne
basse IC apparié >0. Contrôle positif M2 exactement 1 ; lookup réservé dans
±0,04 de 0,50. Conclusion maximale : composition/transfert sur cette tâche,
jamais preuve générale de raisonnement.

## Exécution, preuves et arrêt

```
make experiments/choix-build/IDENTIFIANT-NEUF/choix
python3 experiments/choix/run.py experiments/choix-build/IDENTIFIANT-NEUF/choix experiments/choix-runs/IDENTIFIANT-NEUF
```

Le script refuse un répertoire existant. Il écrit un manifeste SHA256 (ELF,
sources, protocole, candidat, partition) **avant** selftest et campagne. Puis
selftest, M1 et ses critères, M2 si dispositif M1 valide. Les sorties complètes
par phase sont compressées après calcul, accompagnées de leur SHA256.
Les résultats par graine, IC, tous les échecs et verdicts sont conservés.
Une exception interrompt avec échec ; aucun résultat partiel n'est un succès.

Règle initiale v1 (résultats conservés ; corrigée pour la phase distincte E1
dans [ECOLOGIE.md](ECOLOGIE.md)) : le passage à une écologie exige un dispositif
valide ET un candidat démontrant l'usage d'information : sans cela on s'arrête
au résultat négatif du banc et on ne lance pas un monde présenté comme validé.
Cette règle d'arrêt est fixée ici avant toute mesure. Cette campagne n'entraîne
aucun génome ; elle ne peut donc pas établir une émergence par sélection.
Le monde écologique n'est pas implémenté tant que cette porte n'est pas passée.
La phase prospective E1 applique désormais la porte du plan validé : dispositifs
M1/M2 valides, puis test de sélection par survie/reproduction, sans prétendre
que le candidat general est déjà adapté.
