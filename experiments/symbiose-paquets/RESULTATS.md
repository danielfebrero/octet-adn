# Résultat : NON, 1 graine sur 32 passe, 24 étaient requises

Pré-enregistrement committé dans `2bc1fad`, avant l'exécution des selftests et
avant toute mesure. Le protocole de `README.md` est resté figé. Deux passages
complets du même ELF donnent les **228 mêmes lignes à l'octet près**, y compris
les ADN finaux et les hashes de population. Aucun mécanisme n'a été ajouté après
les chiffres.

**Phrase testée.** Transmettre ensemble deux catalyseurs complémentaires peut
faire évoluer un métabolisme conditionnel que le brassage libre défait.

**Seuil.** Sur au moins 24 graines sur 32 : rendement du paquet ≥ 0,90, avantage
sur brassage ≥ 0,20, et rendement seul/partenaire ablaté ≤ 0,10. Bruit et cristal
peuvent atteindre 0,90 sur au plus 2 graines chacun. Tous les rendements se
calculent sur 8192 occasions par graine (64 paires × 128 substrats réservés).

## Comptes

| Bras | Produits / occasions, 32 graines | Rendement moyen | Minimum | Maximum | Graines ≥ 0,90 |
|---|---:|---:|---:|---:|---:|
| Paquet | 104025 / 262144 | 0,396824 | 0 | 0,935425 | 1 / 32 |
| Brassage | 112049 / 262144 | 0,427433 | 0,132813 | 0,590576 | 0 / 32 |
| Bruit sans sélection | 119 / 262144 | 0,000454 | 0 | 0,014526 | 0 / 32 |
| Cristal EMIT constant | 0 / 262144 | 0 | 0 | 0 | 0 / 32 |

Les essais seul et partenaire ablaté donnent **0 / 262144**, dans chacun des
quatre bras. C'est attendu par la chimie imposée ; cela vérifie le calcul et ne
prouve pas une dépendance qui serait apparue spontanément.

Le paquet dépasse le brassage sur 13 graines sur 32. Sa marge atteint 0,20 sur
7 graines ; le rendement 0,90 n'est atteint que sur une graine, la 15, qui passe
aussi la marge. Les 31 autres ratent le critère conjoint. La graine 31 atteint
0,897705 : elle reste un échec au seuil pré-enregistré de 0,90.

Chaque compte ci-dessous a le dénominateur **8192**. « Passe » applique exactement
la conjonction pré-enregistrée, pas un test décidé après coup.

| Graine | Paquet | Brassage | Bruit | Cristal | Passe |
|---:|---:|---:|---:|---:|---|
| 0 | 2360 | 1240 | 0 | 0 | non |
| 1 | 1825 | 3525 | 0 | 0 | non |
| 2 | 7261 | 4102 | 0 | 0 | non |
| 3 | 1720 | 3602 | 0 | 0 | non |
| 4 | 0 | 4443 | 0 | 0 | non |
| 5 | 1830 | 1088 | 0 | 0 | non |
| 6 | 2016 | 3020 | 0 | 0 | non |
| 7 | 2198 | 3838 | 0 | 0 | non |
| 8 | 1827 | 3716 | 0 | 0 | non |
| 9 | 0 | 4326 | 0 | 0 | non |
| 10 | 6945 | 3561 | 0 | 0 | non |
| 11 | 2242 | 3162 | 0 | 0 | non |
| 12 | 1674 | 2796 | 0 | 0 | non |
| 13 | 1750 | 4687 | 0 | 0 | non |
| 14 | 1992 | 3529 | 0 | 0 | non |
| 15 | 7663 | 3102 | 0 | 0 | oui |
| 16 | 1903 | 3903 | 0 | 0 | non |
| 17 | 7037 | 3270 | 0 | 0 | non |
| 18 | 2094 | 3864 | 0 | 0 | non |
| 19 | 2565 | 3385 | 0 | 0 | non |
| 20 | 2172 | 2931 | 0 | 0 | non |
| 21 | 3423 | 3332 | 0 | 0 | non |
| 22 | 1980 | 4257 | 0 | 0 | non |
| 23 | 4617 | 4084 | 0 | 0 | non |
| 24 | 2072 | 4608 | 0 | 0 | non |
| 25 | 7113 | 2629 | 0 | 0 | non |
| 26 | 3465 | 3902 | 119 | 0 | non |
| 27 | 1917 | 3809 | 0 | 0 | non |
| 28 | 6855 | 4262 | 0 | 0 | non |
| 29 | 4053 | 3760 | 0 | 0 | non |
| 30 | 2102 | 1478 | 0 | 0 | non |
| 31 | 7354 | 4838 | 0 | 0 | non |

## Verdict et limites

**NON.** Le dispositif valide ses témoins et son contrôle positif, mais le
paquet ne donne pas la robustesse prédite à 512 générations : **1/32 pour 24/32**
requis. Le brassage a même un rendement moyen supérieur de 0,030609 à ce budget.
Ce dernier écart est descriptif, sans prétention de supériorité statistique.

Une seule paire construite à la main donne 8192/8192 dans le selftest, mais elle
n'entre jamais dans les populations évoluées. La chimie offre donc une solution,
et la population aléatoire l'atteint assez largement sur une seule graine. Le
résultat ne dit pas que toute sélection collective échoue, seulement que ce
mécanisme, ce budget et cette initialisation échouent au seuil choisi.

Le milieu impose le besoin de deux réactions et le rééchantillonnage impose la
reproduction. Ni la dépendance, ni la copie, ni une vie autonome ne sont donc des
émergences démontrées. Les impulsions réservées vérifient une généralisation de
la réaction à des substrats non sélectionnés, pas un transfert à une autre tâche.
Les occasions au sein d'une même graine sont corrélées : les unités du verdict
sont les 32 graines. La population de cristal est stationnaire par construction
(sans mutation et sans sélection), et n'exige aucune boucle de générations.

## Validation et preuves

- Compilation `gnu11 -O2 -Wall -Wextra -Werror -Wno-format-truncation`, libc/libm,
  VM historique liée directement ; aucun source moteur modifié par ce dossier.
- Selftest vert : positif 8192/8192 ; seul, ablation et cristal à 0 ; STORE ne
  touche pas l'ADN d'entrée ; RNG du bac intact ; hash d'un monde extérieur
  inchangé ; deux évaluations identiques ; permutation conservant les 128 membres.
- Même selftest vert sous AddressSanitizer + UndefinedBehaviorSanitizer.
- Deux mesures intégrales identiques par `cmp`, populations et ADN inclus.
- Aucun ELF, répertoire de run, checkpoint ou TSV dans Git ; aucun monde vivant
  consulté, arrêté ou relancé.

ELF neuf : `/tmp/octet-symbiose-0996a2cd.qoslfnFD/symbiose`.
SHA-256 : `54925ab2a3f3f06d8aa270208364305dd90c68b3156d7f7be1c61890973c8618`.

Preuve brute (tableaux, 64 paires finales imprimées, hashes et verdict) :
`/tmp/octet-symbiose-0996a2cd-results.txt`.
SHA-256 : `39a6780eb227afbd9d5181da243d8e6965eb92232ac74921a4b36de1edf2689f`.

Rejeu : `/tmp/octet-symbiose-0996a2cd-replay.txt`, même SHA-256.
Selftest instrumenté : `/tmp/octet-symbiose-0996a2cd-asan.txt`.

Pour reproduire depuis la racine :

```sh
experiments/symbiose-paquets/run.sh > /tmp/symbiose-nouvelle-mesure.txt
```

Le script crée à chaque appel un chemin de build `/tmp` jamais utilisé. Il
n'appelle pas `make`, ne charge aucun checkpoint, n'écrit pas dans un monde et
ne lance aucun mode soupe, fuzz ou mix.
