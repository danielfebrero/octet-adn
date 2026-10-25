# Transplant — résultats de la série préenregistrée (25 octobre 2026)

ELF `experiments/transplant-build/b95ade8/transplant` (protocole `b95ade8`), 230 expériences, toutes terminées,
aucun échec. Mondes vivants lus en lecture seule, aucun arrêté ni modifié. Runs sous
`experiments/transplant-runs/serie-b95ade8/` (hors git), analyse `transplant/analyse.sh`.

## A. Jardin réciproque — adaptation locale

Part finale de la lignée X (graines 1001→1010), sous les règles de X | sous les règles de Y :

| paire | parts par graine | LA > 0 | verdict |
|---|---|---:|---|
| monde / forces | 0\|0 ×7, 1\|0 ×3 | 3/10 | non |
| monde / causes | 1\|1 ×5, 1\|≈0 ×5 | 5/10 | non |
| **forces / causes** | **1\|0,01–0,05 dans les 10** | **10/10** | **tenu** |

- **forces / causes : adaptation locale réciproque nette.** Chacun élimine l'autre chez lui, dans toutes les
  graines. Les lignées de forces (ATTACK) et de causes ont été sculptées par leurs règles au point de perdre
  chez le voisin.
- **monde / forces : supériorité générale, pas locale.** La lignée de forces gagne aussi chez monde (7/10).
- **monde / causes : contingence.** Monde gagne toujours chez lui ; chez causes, tout ou rien selon la graine (5/5).
- Dérive (un monde contre lui-même, 64 fondateurs) : parts finales de 0,05 à 0,85 (monde), 0 à 1 (forces),
  0,40 à 0,54 (causes). Le jardin dérive fort ; seul un résultat à 9–10/10 sort du hasard.

**Constat hors préenregistrement :** sous les règles de monde, quand la lignée de forces l'emporte, le jardin ne
porte plus que 198–210 vivants, contre 931–958 quand monde l'emporte (et 942–954 dans le témoin monde contre
monde). **Le gagnant de la compétition appauvrit le monde** : il exclut l'autre sans mieux exploiter le milieu.

## B. Tournoi — famille division, physique commune (règles de base de sénescence)

Part de la première lignée nommée, 10 graines : parenté 0,875 contre rencontres ; capture 0,742 contre rencontres ;
sénescence 0,578 contre parenté ; capture 0,222 contre parenté ; sénescence 0,877 contre rencontres ; capture
0,165 contre sénescence. **Hiérarchie transitive : sénescence > parenté > capture > rencontres.** Aucune fixation
complète en 20 000 ticks. Descriptif : l'extension rencontres (échange, sénescence, capture, parenté) est absente
du jardin, les lignées se mesurent sur la physique de base.

## C. Téléportation — fourches de mondes vivants

| source | arrivants du même monde (part finale) | arrivants étrangers | écart > 0 | verdict |
|---|---:|---:|---:|---|
| vie-monde | 0,102 | 0,000 | 10/10 | **tenu** |
| vie-causes | 0,090 | 0,064 | 10/10 | **tenu** |

- Dans monde, les 10 % d'arrivants de causes **disparaissent tous** ; les 10 % de monde gardent leur part.
- Dans causes, les arrivants de monde tiennent mieux (6,4 %) mais toujours moins que ceux de causes.
- **Retrait de la moitié** des vivants : après 10 000 ticks, population égale à celle du témoin (0,997 et 1,000).
  Les deux mondes sont résilients à une perte massive.

## Lecture

Les mondes ont façonné leurs habitants : un génome n'est pas « bon » en soi, il est bon **chez lui** (forces /
causes, téléportation). Les rencontres ne produisent pas de fusion ni de coexistence : un côté exclut l'autre,
parfois en appauvrissant le monde. Limites : jardins à un îlot sans migration, physique de base pour la famille
division, fourches continuées par le moteur de base (même moteur pour témoin et perturbations).
