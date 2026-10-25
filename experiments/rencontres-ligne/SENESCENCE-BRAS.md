# Bras sénescence — préenregistrement (débat 4881cc62, 2026-10-25)

Question : la montée de l'octet de vie (len-2) dans `vie-senescence-3a1923e` est-elle portée par les
naissances, et suit-elle le levier âge max ? Le monde vivant reste l'archive : rien n'y est relancé
ni réécrit. Ce protocole est commité avant toute mesure de la série ; le pilote technique (graine 101,
20 000 ticks, un seul monde par bras) n'a servi qu'à vérifier que les colonnes s'écrivent.

## Bras (même binaire, même graine, seule la règle d'âge change)

| profil | âge max |
|---|---|
| `senescence` | `100 + 8 × octet(len-2)` (règle actuelle) |
| `senescence-decouple` | `100 + 8 × 128 = 1 124` pour tous (128 fixé d'avance : moyenne initiale 128,6) |
| `senescence-inverse` | `100 + 8 × (255 − octet(len-2))` |

Limite annoncée : le bras découplé change aussi la démographie (âge max moyen différent).

## Mesure

`vivre.tsv`, colonnes ajoutées : `births birth_life birth_low birth_n1 birth_n4 birth_n6 birth_n10`
(nés depuis le relevé précédent : âge 0, parent connu, fondateurs exclus ; octet lu à la naissance,
avant toute mort), `young*` (âge < 100), `n1 n4 n6 n10` (octets len-1, len-4, len-6, len-10 : distances
1, 2, 4, 8 du locus). Ces quatre témoins sont des diagnostics corrélés, pas des répétitions.

## Série

- 20 graines indépendantes 1001…1020, chacune dans les 3 bras (60 mondes), recette des mondes vivre :
  `--islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1`, `--ticks 100000`.
- Statistique par monde : M = moyenne de `birth_life` pondérée par `births` sur les relevés de
  80 000 (exclu) à 100 000 ticks. Unité statistique : le monde. Même graine = même population fondatrice.
- Contrastes principaux, appariés par graine :
  D1 = M(actuel) − M(découplé), prédit > 0 ; D2 = M(inversé) − M(découplé), prédit < 0.
- Critère : intervalle t apparié (19 ddl) à 97,5 % pour chacun (Bonferroni sur deux contrastes),
  entièrement du côté prédit. Les deux → « la valeur de l'octet à la naissance suit le levier âge max ».
  Un seul → résultat partiel, dit tel quel. Aucun → non.
- Diagnostics, sans décision : mêmes contrastes sur les 4 octets témoins à la naissance ; `birth_low`
  (part d'octets < 32, exécutables comme opcode) ; mondes éteints (pop 0) comptés et exclus avec leur paire.
- Vingt graines ne garantissent pas la puissance. Un intervalle qui touche 0 reste incertain.

Ce que le résultat ne dira pas : l'optimum du bras actuel est inscrit dans la règle (vie gratuite) ;
ce n'est pas un compromis d'histoire de vie. Le bras coût d'entretien est une question séparée.

Lancer : `sh experiments/rencontres-ligne/senescence-bras.sh ELF RACINE` ; analyser :
`sh experiments/rencontres-ligne/senescence-bras-analyse.sh RACINE`.

## Résultat (2026-10-25, ELF `claude-d5a2462`, 60 mondes, aucun éteint)

Mesure (a), lecture seule d'une copie du checkpoint de l'archive (t = 4 858 000) : 12 vivants d'âge < 100,
octet de vie moyen 234,4 ; 51 vivants d'âge ≥ 1 500, 254,8. Pas de journal de naissance dans ce
binaire : l'archive ne dit rien de plus. Parenté et capture : aucun vivant de moins de 1 500 ticks.

Série préenregistrée, fenêtre 80 000–100 000 ticks, octet à la naissance M :

| bras | M (moyenne des 20 graines) | part < 32 (opcode) |
|---|---|---|
| actuel | ≈ 233 (195 à 238) | ≈ 0,08 |
| découplé | ≈ 45 (27 à 66) | ≈ 0,77 |
| inversé | ≈ 6 (4,7 à 9,1) | ≈ 0,97 |

- D1 actuel − découplé : +188,6, IC 97,5 % [179,3 ; 197,9] → **prédit > 0, tenu** (20/20 graines).
- D2 inversé − découplé : −38,9, IC 97,5 % [−45,5 ; −32,3] → **prédit < 0, tenu** (20/20 graines).
- Verdict préenregistré : la valeur de l'octet à la naissance suit le levier âge max, dans les deux sens.
- Diagnostics : les 8 contrastes sur les témoins len-1, len-4, len-6, len-10 ont tous un IC qui contient 0.
- Hors préenregistrement (dit comme tel) : moyenne des vivants − moyenne des naissances sur la même
  fenêtre = +17,8 (actuel, 20/20 > 0), −0,4 (découplé), −3,3 (inversé). La survie différentielle existe
  et gonfle la moyenne des vivants d'environ 18 points ; l'essentiel de l'écart est déjà à la naissance.
- Sans levier, l'octet len-2 ne reste pas à 128 : il descend vers ~45 et devient un opcode exécutable
  chez ~77 % des nouveau-nés. La base de comparaison est le bras découplé, pas la valeur de départ.
- Limite : l'inversé bute sur le plancher 0 ; son contraste est borné. L'optimum du bras actuel est celui
  que la règle inscrit (vie gratuite). Le coût d'entretien reste une question séparée.
