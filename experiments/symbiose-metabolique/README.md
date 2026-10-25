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

Compilation et résultats seront ajoutés après le commit de ce document.
