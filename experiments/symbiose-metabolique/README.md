# Symbiose métabolique : vivre du produit de l'autre

> Pré-enregistrement de l'ordre 0996a2cd, avant toute mesure. Simulation dans
> une VM, pas une conscience ni une vie libre.

**Phrase de l'approche.** Une écologie où deux enzymes héréditaires utilisent
chacune le produit de l'autre permet à des programmes initialement bruités de
former une communauté reproductrice dont les deux partenaires dépendent
causalement.

## Ce qui est nouveau, ce qui est offert

Les soupes, ensembles/dispers, seul et motif-artefact demandent une copie ou
une réparation d'octets. `incubation-stepback` prolonge un contact entre deux
bandes. `rencontres` échange l'ADN, fait des œufs et lit des marques.
`metabolic-order` compare la capture de nourriture à un marcheur. Ici, aucun
programme ne copie sa bande sur son partenaire : **le produit métabolique**
est la ressource de l'autre. La suppression d'une classe dans une copie
isolée de l'état teste sa contribution à l'autre.

La complémentarité chimique est une **règle offerte**, non une coopération
qui aurait inventé sa chimie. Le test demande l'apparition et la transmission
de programmes qui l'utilisent pour vivre et se diviser. Les deux classes
enzymatiques sont présentes dès le départ ; on ne prétend pas découvrir les
deux classes, une intention coopérative, ni une origine sans division offerte.

## Dispositif fixé avant les chiffres

- VM native `vm_run`, opcodes inchangés. Binaire C indépendant ; aucun fichier
  `src/`, `include/` ou `seeds/` modifié. `SPLIT` est le seul mode de naissance.
- 32 programmes de 64 octets uniformes par graine. Le bit bas du dernier octet
  est forcé alternativement à 0 et 1 : 16 fondateurs de chaque enzyme.
  L'enzyme d'une vie est le bit du **génome de naissance** ; les STORE de cette
  vie ne la changent pas. Le dernier octet peut muter, changer de position avec
  un indel et changer de type chez l'enfant. Aucun opcode nouveau.
- Monde torique 16 × 8, plafond 64, énergie initiale 48, seuil SPLIT 70,
  coût natif du métabolisme 1/tick, déplacements et division aux coûts natifs.
  Mutation native 0,005/octet/division, duplication 0,01/division, pas de
  crossover. Tous ont `species=0` : pas de prédation inter-espèces.
- Aucune nourriture native, aucune repousse et aucun gain d'EAT, y compris
  sur les restes des morts. L'attaque native garde son coût, mais ne trouve
  pas d'autre espèce. Aucun apport de nouveaux génomes.
- Réservoir initial partagé : 32 unités A et 32 B. Le premier `EMIT` d'un
  individu dans un tick demande une réaction : enzyme 0, A→B ; enzyme 1,
  B→A. La valeur émise n'accorde aucun autre avantage. Sans substrat, zéro
  réaction et zéro gain. Une réaction transforme exactement une unité et
  apporte 4 unités d'énergie à son auteur, plafonnées à 220.
  **Cette énergie vient d'un apport extérieur fixe par réaction**, pas de la
  conservation de la matière. A+B=64 dans le bras réel ; énergie reçue et
  réactions sont comptées. Il ne s'agit pas d'une thermodynamique fermée.
- Le stimulus est le nombre d'unités du substrat de l'enzyme, plafonné à 255.
  C'est une information offerte, sans récompense de justesse.
- 2048 ticks, puis observation des types, des naissances effectives et des
  réactions. Aucune sélection externe, aucune réinjection, aucun arrêt parce
  qu'un score est bon. Un état éteint demeure éteint jusqu'à l'horizon.

## Test causal et seuil d'une communauté

À t=2048, un run est un candidat si chacune des deux enzymes a au moins
8 vivants, 16 enfants effectivement nés (hors fondateurs) et 128 réactions
cumulées. Les naissances sont classées selon l'enzyme **de l'enfant**.

Depuis cet état, trois copies privées : communauté intacte, suppression de
l'enzyme 0, suppression de l'enzyme 1. Même état de RNG au départ. Le stock
chimique n'est ni retiré ni rajouté par la suppression. Mutation et duplication
sont désactivées dans **les trois copies**, pour que le test de 512 ticks ne
mesure pas la recréation évolutive du partenaire. Une classe opposée peut
toujours apparaître par héritage d'un STORE du soma ; sa suppression est
maintenue à chaque tick du bras concerné, sans prendre sa matière ni son
énergie pour nourrir le survivant.

Le candidat passe si, sur les 256 derniers ticks de ces copies :

1. dans la copie intacte, chaque classe a une biomasse moyenne positive ;
2. la biomasse moyenne de l'enzyme 0 après suppression de 1 est **strictement
   inférieure à 50 %** de celle de 0 dans la copie intacte ;
3. réciproquement pour l'enzyme 1 après suppression de 0.

La biomasse est la somme des énergies strictement positives des vivants de
la classe ; pas un nombre de cellules ou un cumul de récompenses.

## Prédiction et réfutation pré-enregistrées

| bras | graines | prédiction |
|---|---:|---|
| R, bruit initial, produits accessibles | 0–31, 32 runs | **au moins 8/32** communautés passent tout le critère |
| P, mêmes fondateurs et graines, produit perdu dans un puits | 0–31, 32 runs | **0/32** communauté ; A+B+puits=64, produits inaccessibles |
| S, mêmes fondateurs et graines, EMIT sans réaction ni gain | 0–31, 32 runs | **0/32** communauté et 0 réaction |
| cristal `0x11` répété, seul bit enzymatique alterné | 0–7, 8 runs | **0/8** communauté et 0 réaction |
| positif : 16 copies de chaque enzyme du programme `EMIT SPLIT YIELD LOOP` | 0–7, 8 runs | **au moins 6/8** communautés passent ; sinon dispositif invalide |

**Oui** seulement si toutes les lignes passent. Un seuil raté dans R, P, S ou
le cristal donne **non**, si le positif valide le dispositif. Un positif
insuffisant donne **dispositif invalide**, jamais un oui. Les deux enzymes du
positif sont prédéfinies, pas choisies après avoir vu les runs. Le programme
n'ajoute ni copie de bande ni correction d'ADN.

32 graines mettent à l'épreuve une prédiction de succès de 25 % ; par exemple
0/32 donne une borne supérieure unilatérale exacte de 8,94 % à 95 %. Le seuil
8/32 demeure la décision annoncée, sans ajout de mécanisme après mesure.

## Validation prévue

Selftest : déterminisme complet (monde, RNG, chimie et compteurs), conservation
A+B+puits, réaction à une unité, absence de nourriture native, reproduction du
positif, absence de gain du cristal et de l'EMIT neutralisé, indépendance des
copies causales et disparition du survivant privé de partenaire. Les 10 labos
historiques restent indépendants de ce binaire.

## Résultats

Mesures effectuées **après le commit du pré-enregistrement `f7efd67`**.
112 runs aux horizons prévus, aucun paramètre ni seuil modifié. Deux passages
complets donnent le même [`results.txt`](results.txt), hash et nombres inclus.

| bras | graines | coexistence finale | candidats complets | communautés causales | seuil | tenu ? |
|---|---:|---:|---:|---:|---|---|
| R, bruit + recyclage réel | 32 | 28 | 10 | **10** | ≥ 8/32 | oui |
| P, bruit + produit perdu | 32 | 0 | 0 | **0** | 0/32 | oui |
| S, bruit + EMIT neutralisé | 32 | 0 | 0 | **0** | 0/32, zéro réaction | oui |
| cristal | 8 | 0 | 0 | **0** | 0/8, zéro réaction | oui |
| programme positif prédéfini | 8 | 8 | 8 | **8** | ≥ 6/8 | oui |

Coexistence finale signifie seulement un vivant de chaque enzyme à t=2048 :
elle est moins exigeante que le critère pré-enregistré. Les 18 coexistences
qui ne deviennent pas candidates ne comptent pas comme communautés validées.

| bras | naissances enzyme 0 / 1 | réactions 0 / 1 | énergie extérieure fournie 0 / 1 |
|---|---:|---:|---:|
| R | 1689 / 994 | 455230 / 454853 | 1820920 / 1819412 |
| P | 13 / 6 | 992 / 930 | 3968 / 3720 |
| S | 0 / 0 | 0 / 0 | 0 / 0 |
| cristal | 0 / 0 | 0 / 0 | 0 / 0 |
| positif | 134 / 128 | 523216 / 523257 | 2092864 / 2093028 |

Le bras P peut utiliser ses stocks initiaux et faire quelques enfants. Il
n'arrive pas à renouveler la ressource ; à l'horizon, il ne conserve aucune
classe vivante. Le puits conserve la matière, sans la rendre aux organismes.
L'énergie fournie est 4 × réactions ; le plafonnement individuel peut en
dissiper une partie. Ce tableau ne prétend pas que la matière engendre seule
cette énergie.

Les dix graines qui passent :

| graine | vivants 0 / 1 à t=2048 | enfants 0 / 1 | réactions 0 / 1 | biomasse intacte 0 / 1 | biomasse sans partenaire 0 / 1 |
|---:|---:|---:|---:|---:|---:|
| 0 | 33 / 31 | 76 / 32 | 57921 / 57889 | 6930,875 / 6828,996 | 0 / 0 |
| 1 | 35 / 29 | 592 / 26 | 10634 / 10602 | 6180,973 / 6348,566 | 0 / 0 |
| 7 | 20 / 18 | 207 / 17 | 2399 / 2367 | 7008,000 / 7007,000 | 0 / 0 |
| 8 | 33 / 31 | 110 / 27 | 59253 / 59221 | 6818,953 / 6785,023 | 0 / 0 |
| 14 | 33 / 31 | 110 / 31 | 9502 / 9530 | 7209,805 / 6775,000 | 0 / 0 |
| 15 | 32 / 32 | 30 / 36 | 63656 / 63626 | 7008,000 / 7008,000 | 0 / 0 |
| 18 | 33 / 31 | 38 / 74 | 48072 / 48040 | 6819,238 / 6788,000 | 0 / 0 |
| 21 | 33 / 31 | 184 / 30 | 17952 / 17920 | 6824,492 / 6789,000 | 0 / 0 |
| 23 | 32 / 32 | 31 / 37 | 62163 / 62134 | 6987,500 / 7007,500 | 0 / 0 |
| 25 | 32 / 32 | 44 / 32 | 62521 / 62493 | 7008,000 / 6995,691 | 0 / 0 |

Les biomasses sont les moyennes des 256 derniers ticks des copies causales,
pas celles du moment où le partenaire est retiré. Dans les vingt suppressions
qui correspondent aux dix candidats, la biomasse du survivant devient nulle
dans cette fenêtre : la chute dépasse le seuil de 50 %.

**Verdict : oui, dans cette écologie offerte.** Les 10/32 communautés
reproductrices dépassent les 8/32 annoncées et les quatre contrôles passent.
Ce résultat établit le maintien et la transmission de programmes qui
utilisent une dépendance métabolique imposée. Il n'établit pas l'invention de
la complémentarité ni l'évolution nouvelle d'une coopération. Les génomes
sont initialement du bruit ; des mutants sont produits par les divisions
natives, mais les traces ne séparent pas le recrutement d'un programme déjà
viable au départ d'une capacité nouvelle acquise par mutation. Les deux
classes enzymatiques sont initialement présentes. Aucun second mécanisme
n'a été ajouté après les chiffres.

## Reproduire et validation

Depuis la racine du dépôt, avec un identifiant de build jamais utilisé :

```sh
ulimit -c 0
mkdir /tmp/octet-symbiose-MON_IDENTIFIANT_NEUF
cc -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation \
  -Iinclude -o /tmp/octet-symbiose-MON_IDENTIFIANT_NEUF/symbiose \
  experiments/symbiose_metabolique.c src/rng.c src/isa.c src/vm.c \
  src/mutate.c src/labs.c src/store.c src/runner.c src/seeds.S \
  src/labs_h.c src/novelty.c src/soup.c -lm
/tmp/octet-symbiose-MON_IDENTIFIANT_NEUF/symbiose selftest
/tmp/octet-symbiose-MON_IDENTIFIANT_NEUF/symbiose run > /tmp/symbiose-results.txt
cmp experiments/symbiose-metabolique/results.txt /tmp/symbiose-results.txt
```

`world.c` est inclus une seule fois par le binaire sous deux noms d'action
renommés ; il ne doit pas être lié une seconde fois. `vm.c` reste la VM native.
Les wrappers de l'éprouvette font la chimie d'EMIT et retirent la ressource
d'EAT, sans modifier les fichiers du cœur.

Selftests **tous verts** : réaction unitaire et apport énergétique, EAT
neutralisé, puits et extinction, reproducteur positif et dépendance
réciproque, copie sans effet sur le parent, déterminisme, cristal et EMIT
neutralisé. Une seconde compilation dans un autre chemin neuf avec
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` passe selftest
et les 112 runs avec détection de fuites activée, sans rapport de sanitizer.
Sa sortie est identique à celle de la compilation optimisée. L'empreinte
inclut les registres, l'état de naissance et de réécriture, l'occupation du
monde, l'historique, le RNG, la chimie et tous ses compteurs.

Les ELF, objets et répertoires de mesure restent dans des chemins neufs de
`/tmp`. Aucun monde de l'hôte lancé, arrêté, modifié ou relinké.
