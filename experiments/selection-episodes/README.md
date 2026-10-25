# Sélection par épisodes, sans moyenne qui masque les spécialistes

## Pré-enregistrement — ordre 0996a2cd

**Phrase de l'approche.** Une sélection lexicase, qui confronte les génomes à des
épisodes d'apprentissage dans un ordre aléatoire plutôt qu'à leur note moyenne,
peut faire évoluer un même programme capable des deux mémoires m2 et m3.

C'est une modification de la sélection **hors runner**, sans nouvel opcode,
registre, opérateur de naissance, génome conçu ou modification du moteur.
`general-joint-negative` sélectionne une moyenne ou une mémoire directement ;
`novelty-map-elites` archive des descripteurs. Ici chaque épisode peut éliminer
des candidats avant que les autres épisodes ne les départagent. Le critère reste
celui du joint déjà raté, pas une diversité de spécialistes rebaptisée réussite.
Les registres existants du moteur restent à leur valeur par défaut, identique
pour tous les bras. Aucun `delay`, `unmix` ou `deepen` n'est appelé.

## Dispositif figé avant mesure

- 32 graines indépendantes : 0 à 31. Trois bras appariés : lexicase `L`, moyenne
  `M`, sélection neutre `N` (témoin bruit de sélection).
- Population de 16, initialement 16 copies de la seed `general` inchangée.
- 128 générations ; à chaque génération, 16 enfants de parents tirés
  uniformément dans le pool, avec `mutate()` réel, taux 0,005 / duplication 0,01.
  Les 16 parents et les 16 enfants sont évalués sur les **mêmes quatre épisodes**.
- Deux épisodes mémoire-2 et deux mémoire-3 : `gn_eval`, held=0, level=0,
  ntask=1, mode normal, familles 6 et 7. Leurs graines changent à chaque
  génération ; elles sont identiques entre bras, sans lecture du jeu réservé.
- L : pour chacune des 16 places, permutation neuve des quatre épisodes,
  filtrage à la meilleure justesse moins 1/32 (une erreur sur les essais notés),
  puis choix uniforme parmi les survivants. Tirages avec remise, pas d'élite
  ajoutée. M : les 16 meilleurs par moyenne des quatre notes, égalités
  départagées par une permutation. N : 16 tirages uniformes avec remise.
- Même nombre d'enfants et d'évaluations par bras. Les flux de mutations,
  d'épisodes et de sélection sont distincts. Des génomes de longueurs différentes
  peuvent consommer des nombres de tirages mutationnels différents.
- Mesure **seulement à la fin**, sur les 16 génomes de chaque pool : les deux
  mémoires sur les 40 tâches réservées de `gn_check`. Aucun résultat réservé
  n'est utilisé pour sélectionner ou arrêter un run.

Un génome est joint si m2 ET m3 >= 0,95 et si chacune chute d'au moins 0,5
quand le génome est gelé ET quand la récompense est masquée. Le code retourné
par `gn_check` seul ne suffit pas : il peut annoncer une seule des mémoires.
Une graine passe si au moins un génome final est joint. Les maxima de m2 et m3
provenant de deux individus différents ne comptent jamais comme joint.

## Prédiction et réfutation

**Oui** seulement si L passe sur **au moins 8/32 graines** ET dépasse M d'au
moins **6 graines** ET N passe sur **au plus 1/32**. Un seul seuil raté donne
**non à ce budget** (2048 enfants par graine), sans conclure à l'impossibilité.
Avec 0/32, la borne supérieure binomiale unilatérale à 95 % du taux par graine
est 8,94 %, sous le taux prédit de 25 %. Le budget de générations reste une
limite explicitement assumée, pas un paramètre ajusté après les comptes.

Témoins supplémentaires du détecteur : 32 génomes uniformément bruités de
172 octets et les 32 homopolymères de valeurs 0 à 31 doivent tous être non
joints. Un seul faux positif invalide le dispositif. Ces contrôles sont mesurés
sans évolution : faire muter le cristal en ferait un nouveau bruit. Le témoin
positif de **logique du critère** est un vecteur de notes construit ; il ne
prouve pas qu'un génome joint est atteignable dans cette ISA. Le selftest
vérifie aussi les spécialistes exclus par la moyenne, le filtrage lexicase,
l'inertie du jeu réservé et le déterminisme d'une petite évolution.

Pas de second mécanisme ni de nouveau seuil après mesure. Pas de course `soup`,
`fuzz` ou `mix`. Sources, ELF et sorties ne touchent aucun monde de l'hôte.

## Construction prévue

Binaire C séparé `experiments/selection_episodes.c`. Construire directement les
sources C du cœur dans un chemin de build **inédit**, jamais les objets partagés.
Les commandes exactes, résultats et limites seront ajoutés après le commit de
ce pré-enregistrement. Les résultats bruts seront du texte, sans JSON ni TSV.
