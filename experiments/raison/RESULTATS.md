# Raison — résultats de la série préenregistrée (25 octobre 2026)

ELF `experiments/raison-build/2b003cf/raison` (sha256 `60e8a2fe…`, protocole `2b003cf`), 200 mondes (10 milieux ×
2 bras × 10 graines), 65 536 cycles chacun, tous terminés. Une coupure de session a interrompu la série : les
mondes partiels ont repris à leur checkpoint avec un ELF identique bit à bit (reprise exacte vérifiée par le
selftest). Évaluation finale figée en lecture seule, 128 épisodes par individu. Runs sous
`experiments/raison-runs/serie-2b003cf/` (hors git).

## Verdict préenregistré (Δ = P(info) − P(témoin), IC 99,5 %, 10 graines appariées)

| milieu | P info | P témoin | Δ | IC 99,5 % | verdict |
|---|---:|---:|---:|---|---|
| memoire | 0,576 | 0,500 | +0,076 | [−0,042 ; +0,193] | non |
| composition | 0,628 | 0,500 | +0,128 | [−0,070 ; +0,327] | non |
| inversion | 0,506 | 0,499 | +0,007 | [−0,009 ; +0,023] | non |
| observation | −0,000 | 0,001 | −0,001 | [−0,004 ; +0,002] | non |
| patience | 0,107 | 0,003 | +0,103 | [−0,097 ; +0,304] | non |
| chainage | 0,207 | 0,002 | +0,205 | [−0,051 ; +0,461] | non |
| transfert | 0,662 | 0,499 | +0,164 | [−0,082 ; +0,409] | non |
| signal | 0,499 | 0,499 | +0,000 | [−0,004 ; +0,005] | non |
| **autrui** | **0,702** | **0,502** | **+0,201** | **[+0,179 ; +0,222]** | **TENU** |
| relation | 0,582 | 0,501 | +0,082 | [−0,006 ; +0,169] | non |

**Un milieu sur dix tient le critère : autrui.** Dans toutes les graines, l'observateur apprend à se servir de
l'action d'un autre individu comme d'une information (0,68–0,72 ; l'acteur lui-même est informé à 75 %).

## Lecture graine par graine (exploratoire, déclarée après coup)

Le critère préenregistré est une moyenne ; les résultats sont **bimodaux** : la capacité émerge pleinement dans
certaines graines et pas du tout dans les autres. Valeur principale du bras info, graines 1001→1010, à comparer au
**maximum des 10 témoins** :

| milieu | témoin max | info par graine | émerge |
|---|---:|---|---:|
| memoire | 0,50 | 0,71 · 0,50 · 0,64 · 0,50 · 0,50 · 0,70 · 0,50 · 0,50 · 0,50 · 0,71 | 4/10 |
| composition (couples jamais vus) | 0,50 | 0,50 ×6 · 0,84 · 0,84 · 0,88 · 0,71 | 4/10 |
| transfert (valeurs jamais vues) | 0,50 | 0,93 · 0,50 ×4 · 0,92 · 0,50 · 0,94 · 0,83 · 0,50 | 4/10 |
| relation (valeurs jamais vues) | 0,50 | 0,50 · 0,66 · 0,65 · 0,50 · 0,61 · 0,60 · 0,50 · 0,62 · 0,68 · 0,50 | 6/10 |
| chainage (max 0,5) | 0,01 | 0,39 · 0 · 0,45 · 0 · 0 · 0 · 0 · 0,40 · 0,44 · 0,39 | 5/10 |
| patience | 0,03 | 0,45 · 0 · 0,07 · 0,33 · 0 · 0,24 · 0 · 0,01 · 0 · 0,01 | 4/10 |
| autrui | 0,51 | 0,68 à 0,72 partout | 10/10 |
| inversion, observation, signal | — | au niveau du témoin | 0/10 |

- **Sept milieux sur dix ont produit la capacité dans au moins une population**, toujours bien au-delà du meilleur
  témoin ; trois jamais (apprendre pendant la vie, s'informer à bon escient, communiquer volontairement).
- **Règle, pas table** : pour composition, transfert et relation, la réussite est mesurée sur des entrées
  jamais vues. Le génome dominant de composition-1009 fait 0,998 sur les couples vus et 0,996 sur les réservés.
- **Mémoire à l'optimum** : les lignées qui ont évolué atteignent 0,755 avec un indice fiable à 75 %.
- **Mécanisme lu (memoire-1001)** : 9 instructions utiles sur 64 octets. L'indice ne reste pas dans un registre :
  il change le **chemin du programme** (indice 1 → YIELD et attente, indice 0 → boucle), et c'est la position
  du programme au tick de décision qui porte la réponse.
- Contingence : avec les mêmes règles, la sélection trouve ou ne trouve pas. Les dix mondes vivants (graine 101)
  sont donc chacun **un tirage** ; certains n'évolueront peut-être jamais.

## Limites

Les mécanismes trouvés sont courts (quelques instructions). Ce n'est pas un raisonnement au sens humain : c'est
de l'usage d'information sélectionné, avec généralisation à l'inédit pour trois milieux. Les trois échecs
indiquent ce que cette écologie n'atteint pas encore : l'adaptation pendant la vie, la décision de s'informer,
la communication à deux rôles.

## Mondes vivants (graine 101, conteneur `octet-raison`) — observations, hors préenregistrement

Relevé du 25 octobre vers 16:25 UTC, 98 304 à 131 072 cycles. Évaluation figée sur une **copie** du checkpoint ;
« canal coupé » = la même population évaluée avec l'information remplacée par du bruit (bras témoin).

| milieu | réussite | canal coupé | remarque |
|---|---:|---:|---|
| **signal** | **0,913** | **0,499** | plat à 0,50 jusqu'à 65 536 cycles, puis 0,84 dès 73 728 : la communication a émergé d'un coup |
| transfert (valeurs jamais vues) | 0,944 | 0,500 | règle |
| autrui | 0,728 | 0,504 | |
| relation (valeurs jamais vues) | 0,705 | — | |
| chainage | déverrouille 0,94, juste 0,80 | — | mesure principale ≈ 0,29 (max 0,5) |
| memoire, composition, inversion, observation, patience | ≈ 0,5 | — | pas (encore) émergé |

**Le signal, qui n'a émergé dans aucune des 10 graines de la série, a émergé dans le monde vivant.** Couper le
canal ramène les récepteurs au hasard : ils utilisent réellement ce que les émetteurs envoient. C'est une
observation sur un seul monde, pas un résultat statistique ; elle confirme la contingence vue dans la série.
