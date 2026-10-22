# Ensembles

Idée (Dani) : lois fixes, et dans l'état de la soupe des octets qui réagissent ensemble, quelle que soit leur place. Le génome n'est pas une case qu'on a découpée. On le déduit de l'ensemble.

Lois figées : univers d'origine, adressage B, aucun tirage de table. `src/`, `include/` et `seeds/` ne bougent pas.

## Détecteur, écrit avant la mesure

On oublie les coupures de 64. Sur la bande concaténée, pour chaque décalage de 1 à 32 :

`r(d)` = part des positions où l'octet égale celui d'en face, `d` cases plus loin.

La période `P` est le **plus petit** `d` avec `r(d) ≥ 0,5`. S'il n'y en a pas, il n'y a pas d'ensemble.

Le consensus de chaque phase (le byte majoritaire) est répété jusqu'à 64 octets. Cet assemblage est un génome déduit s'il passe `u_copier` sous les lois fixes. L'ablation dit quels octets réagissent : une substitution les casse.

## Pré-enregistrement

- Bande pure du réplicateur de soupe (`big_b_0`) : `P = 16`, et le consensus est un copieur.
- Cristal périodique qui n'est pas un copieur (`00 11 … ff` répété) : le consensus n'est pas un copieur.
- Bruit pur : aucun `r(d) ≥ 0,5`.
- Évolution : N = 128, 128 époques, une copie du réplicateur dans le bruit, graines 0–3, mutation de fond de l'origine. Le consensus est un copieur sur **au moins 3 graines sur 4**.
- Même évolution sans réplicateur : le consensus n'est pas un copieur.

Un seul seuil raté réfute la déduction à cette échelle. Pas de second détecteur après coup.

## Résultats

Mesuré. Deux passages, même fichier.

| bras | P | r | copieur déduit | essentiels | prise |
|---|---:|---:|---:|---:|---:|
| bande pure | 16 | 1,000 | oui | 8 | — |
| cristal `00 11 … ff` | 16 | 1,000 | non | — | — |
| bruit pur | — | — | non | — | — |
| graine 0 | 16 | 0,992 | oui | 8 | 32 |
| graine 1 | 16 | 0,994 | oui | 8 | 32 |
| graine 2 | 16 | 0,992 | oui | 8 | 32 |
| graine 3 | 16 | 0,978 | oui | 8 | 32 |
| bruit évolué | — | — | non | — | — |

Le consensus des quatre graines est le motif d'origine, octet pour octet : `051d03021c1fff0701030903c906901e`. Les 8 essentiels sont aux phases 1, 7, 8, 9, 10, 13, 14 et 15. Les trois autres copies du cycle, dans la fenêtre de 64, ne réagissent pas.

**Verdict : oui**, à cette échelle. Tous les seuils passent. La période se lit sans les coupures, le cristal périodique n'est pas vivant, le bruit n'a pas d'ensemble, et le vote majoritaire retrouve un copieur sur 4 graines sur 4.

Une seule physique, celle d'origine, et un réplicateur déjà connu. Le détecteur voit un cycle. Un ensemble dispersé, sans période, reste invisible. Sortie : `results.txt`.
