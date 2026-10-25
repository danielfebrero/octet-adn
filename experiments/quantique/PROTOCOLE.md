# Q1 — espace, octets et attention réciproque

Protocole écrit avant les essais du 25 octobre 2026. Moteur autonome, sans
VM, génome, nourriture, reproduction, mort ou objectif. Aucun ancien monde
n'est modifié. Le terme « quantique » nomme ici un modèle hybride inventé,
pas une simulation de physique quantique complète.

## Traduction de la demande

* Trois coordonnées continues dans un cube périodique de côté 8 ; temps en
  ticks, pas d'intégration dt=0,02. 64 octets, valeurs uniformes 0..255,
  positions et vitesses initiales aléatoires. Aucun assemblage préfabriqué.
* L'espace est approché par 4³ points représentatifs. Chaque point porte
  une amplitude complexe sur une phase interne circulaire. Dans le modèle
  idéal, cette phase est continue et sa base de Fourier infinie ; le calcul
  utilise 256 échantillons (résolution testée aussi à 128). Au départ,
  distribution uniforme : aucun point ne possède de valeur classique fixée.
  Le champ est un espace de possibilités, pas une population ajoutée.
* Un seul potentiel d'interaction, extérieur aux octets :
  U(r,a,b) = (1 + 0,5 cos(a-b)) × (e² - 2e) × (1-r²/R²)²,
  e=exp(0,9-r), R=2,5, U=0 hors R. Distance adoucie par
  r=sqrt(distance²+0,01). Sa dérivée produit répulsion à courte distance
  et attraction plus loin. Même noyau octet–octet et octet–point.
* Les octets sont classiques. Le champ évolue par étapes unitaires : phase
  due au même U, puis propagation libre du rotor (Fourier, énergie k²/50).
  Les forces des points sur les octets utilisent l'espérance de U dans leur
  état courant ; les positions/valeurs des octets déterminent le potentiel
  qui modifie en retour les amplitudes des points.
* Une attraction octet–octet doit durer 8 ticks consécutifs pour compter
  comme lien. Les composantes connexes de ces liens sont les assemblages.
  Ce graphe mesure les liens ; il n'ajoute aucune force ni contrainte.
* Attention : au moins deux membres du même assemblage influencent un
  point ; ce point change son moment de phase par rapport à son évolution
  libre sans le potentiel présent des octets, ce qui modifie effectivement
  la force rendue aux membres. La même identité d'assemblage doit boucler
  P ticks consécutifs. Toute interruption remet le compteur à zéro.
* Seul événement discret : à P, projection du point sur une phase tirée
  selon |psi|². Cette phase détermine une valeur d'octet. Au tick suivant,
  l'évolution libre reprend ; le point n'est pas figé éternellement. Il
  faut une nouvelle boucle pour une nouvelle projection. Les octets
  initiaux ne sont ni créés ni détruits par cette projection.

## Limites explicites

L'infini est approché, pas stocké. Le maillage spatial, l'adoucissement,
le potentiel, la propagation libre et la règle de projection sont des
choix de conception. « Une loi » signifie un seul noyau d'interaction,
accompagné d'une cinématique et d'une règle d'observation demandée.
L'intrication complète entre particules et sites n'est pas calculée :
c'est un couplage de champ moyen. Le maillage fixe sert de substrat ;
on ne revendique ni invariance par translation continue ni conservation
de l'énergie lors des projections. Aucune conscience, vie ou intention
n'est déduite du mot attention.

## Comparaison préenregistrée

Graines 101, 202, 303, 404, chacune appariée entre P=4,16,64 et témoin
P=0 (aucune projection). 4096 ticks par essai. Tout publier : projections,
liens durables, taille du plus grand assemblage, durée maximale de boucle,
norme du champ, énergie cinétique. Pas de sélection de graines réussies.
Sensibilité : mêmes graines, P=16, résolution de phase 128 au lieu de 256.
Les différences de fréquence n'établissent pas à elles seules une
organisation nouvelle. Les mesures seront des constats, pas un score de vie.

Vérifications : gradient du potentiel, attraction/répulsion, réciprocité
des paires, normalisation unitaire, probabilités de projection, aucun
événement sans assemblage, déterminisme et reprise exacte d'une sauvegarde.

Après vérification : trois instances indépendantes P=4,16,64, graine 101,
dans un nouveau conteneur limité, ELF immuable, checkpoints atomiques.
Les processus et ELF des 19 expériences précédentes restent intacts.
