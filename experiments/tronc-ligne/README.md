# Tronc

Un monde où les lois déjà vivantes tournent ensemble, plus les trois
stimulations, plus ce qu'on y déposera ensuite.

Allumé au départ : forces 3, stress, attention, traits, dépense,
causales, saison douce (amplitude 0,20, période 40 000). Parent :
vie-traits. Binaire : `experiments/terrarium_tronc`. Les processus
déjà lancés gardent leur binaire.

## Brancher une expérience

Déposer un `quelque-chose.so` dans `experiments/vie-tronc/lois/`.
Au relevé suivant (200 ticks), le processus le charge avec `dlopen` et
appelle `loi_registre`. Le contrat est `loi.h` : version 1, un
échantillon, et des accès à la nourriture, au retrait d'une brique,
à la chaleur, à une marque. Rien à relinker. Un fichier illisible est noté
`loi_rejet` et le monde continue.

Lire une brique n'est pas dans le binaire du tronc qui tourne.
Le champ `lire_brique` est en queue du contrat, sur le fork
`experiments/terrarium_lecture`, dossier `experiments/vie-lecture`.
Un `.so` qui appelle cette lecture ne se dépose pas dans
`vie-tronc/lois/` : l'hôte ancien n'a pas le champ.

`exemple.c` s'annonce une fois. Il n'est pas dans `lois/`.

Cinq lois y sont déposées. Chacune agit une fois par relevé, sans
tirage, et sans retirer de brique.

- `osmo.so` — osmotrophie de la soupe : une unité par île, d'une case
  qui en a plus de 2 vers l'est, si l'est est sous 14. Somme conservée.
- `competition.so` — une unité, de l'île la plus nourrie vers la plus
  pauvre.
- `heritage.so` — marque 1 réécrite sur la case (0,0) de chaque île.
- `memoire.so` — une chaleur sur une case de l'île 0 si la population
  a baissé depuis le relevé précédent. L'événement ne part qu'une fois,
  et se réarme si la population remonte.
- `renaissance.so` — en veille tant que la population est au moins
  1000. En dessous, huit unités sur l'île 0.

`lecture.so` ne retire rien. Sur le fork seulement, il compte une fois
les briques posées et les carrés 2×2 mixtes, écrit l'événement
`lecture`, puis se tait.

Le contrat ne rejoue pas les processus d'origine. Les génomes de la
soupe et de la compétition, les tables de lois locales, et la table
d'opcodes de la recherche d'univers restent dehors. Ces fichiers
portent l'effet qui passe : nourriture, marque, chaleur.
