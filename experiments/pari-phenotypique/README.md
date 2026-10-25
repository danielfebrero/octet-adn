# Pari phénotypique : transmettre une diversité plutôt qu'une réponse unique

Pré-enregistrement de la mission 0996a2cd, écrit avant la mesure.

## Phrase et distinction

Une lignée qui transmet en deux octets ses probabilités de changement de
phénotype peut évoluer vers une diversification stochastique qui protège sa
croissance multiplicative contre des catastrophes imprévisibles.

C'est un nouveau modèle jouet d'évolution, pas une nouvelle ISA ni une origine
spontanée de la vie. Les lectures préalables (README racine ; soupes, univers,
lois locales/gardiens, general/regs/transfert/négatif joint, seul, motif,
soupe-spatiale, rencontres et ses résultats) ont déjà mesuré copie, réparation,
mémoire et rencontres. Ici aucune tâche de mémoire, bande à réparer ou rencontre :
le caractère héréditaire est une **distribution de phénotypes**, et le score est
la croissance géométrique d'une lignée dans un risque extérieur sans signal.
Le moteur et les labos historiques ne sont pas compilés ni modifiés.

## Dispositif figé

Le génome comporte deux octets `a,b`. À chaque pas, une cellule de phénotype 0
passe en 1 avec probabilité `a/256`, une cellule 1 passe en 0 avec probabilité
`b/256`. Il n'existe aucun capteur de l'environnement. On calcule exactement
les fractions d'une colonie infinie, sans bruit d'échantillonnage :
`x0' = x0*(1-a/256)+x1*b/256`, `x1' = 1-x0'`. Puis les cellules du bon
phénotype sont multipliées par 1,5, les autres par 0,05. On normalise après
chaque pas et on additionne le logarithme du facteur total. Initialement
`x0=x1=0,5`. Ce modèle offre la reproduction : seule sa stratégie évolue.

L'environnement débute à pile ou face, puis bascule indépendamment à chaque
pas avec probabilité 1/32. Ses graines sont séparées de celles des mutations.
Le milieu d'entraînement est le même pour les deux bras d'une paire.

- 32 courses indépendantes, graines 0..31.
- 48 génomes uniformes au départ, 128 générations. À chaque génération :
  quatre milieux neufs de 256 pas, score moyen de log-croissance par pas.
- Sélection : les quatre meilleurs sont copiés sans mutation ; les 44 autres
  enfants viennent d'un tournoi de trois parents et subissent une mutation.
- Mutation : un octet choisi à pile ou face ; modification uniforme parmi
  `-8,-1,+1,+8` (saturation à 0 ou 255) ou remplacement uniforme. Le choix
  est uniforme parmi les cinq opérateurs. Aucune recombinaison.
- Bruit héréditaire neutre : mêmes fondateurs, mêmes milieux, 48 évaluations
  par génération et mêmes nombres de copies/mutations ; les parents sont
  choisis indépendamment du score. Les quatre copies non mutées viennent
  aussi de parents aléatoires. Le meilleur score n'est jamais consulté pour
  choisir son génome final. La sélection finale conserve le premier des quatre
  meilleurs ; le témoin conserve sa première copie aléatoire.
- Aucun ajustement après la mesure, aucune nouvelle phase évolutive.

## Évaluation tenue hors sélection et témoins

Pour chaque paire, on évalue les deux génomes finaux sur **16 nouveaux milieux
de 4096 pas**, jamais utilisés en évolution. Le score `L` est la moyenne des
16 log-croissances par pas. Le numéro des graines de validation est un espace
séparé de celui de l'entraînement.

Deux cristaux sont des colonies homogènes qui restent toujours en état 0 ou
toujours en état 1. Le comparateur `C` est le **meilleur des deux** sur le jeu
de validation, oracle favorable au témoin. Le témoin `S` est le même génome
sélectionné mais les changements d'état sont synchrones dans toute la colonie.
Sa log-croissance attendue est calculée exactement en propageant les probabilités
des deux états homogènes : on moyenne les logarithmes des multiplicateurs, et
non le logarithme de leur moyenne. Il garde les mêmes probabilités et le même
risque, sans diversification simultanée. Ces témoins distinguent une diversité
utile d'une stratégie constante ou d'une simple bascule commune.

## Prédiction, seuil et réfutation

Une course passe seulement si, sur validation :

1. ses deux octets finaux sont strictement entre 0 et 255 ;
2. `L_selection - L_bruit >= 0,10` ;
3. `L_selection - C >= 0,10` ;
4. `L_selection - S >= 0,10`.

**Prédiction : au moins 24 courses sur 32 passent les quatre conditions.**
23 ou moins réfutent la prédiction à ce budget ; le résultat reste **non**,
même si un sous-test ou une moyenne paraît favorable. Les 32 lignes sont
publiées, y compris les échecs et les deux octets. Sous un nul de signes
équiprobables indépendants, 24 victoires sur 32 a une probabilité unilatérale
inférieure à 0,004 ; cette borne explique le volume, elle ne remplace pas les
marges de 0,10 ni les trois comparateurs.

Selftest préalable : conservation des fractions, phénotype 0/1 conservé sans
bascule, calcul sur un pas à la main, synchronisation équivalente au cristal
quand il n'y a aucune bascule, avantage arithmétique/géométrique d'une colonie
50/50 sur un environnement équilibré, déterminisme des milieux et de
l'évolution réduite. Si un étalon casse, aucun verdict expérimental n'est rendu.

## Reproduire

```sh
sh experiments/pari-phenotypique/run.sh /tmp/pari-IDENTIFIANT_NEUF
```

Le script refuse un chemin qui existe déjà, construit un ELF C distinct dans
ce chemin neuf, lance `selftest`, puis `run`. Les sorties texte et le manifeste
SHA-256 restent dans ce répertoire ; aucune donnée de run ni ELF n'est suivie
par Git. Libc et libm seulement, aucun Python ni JSON.
