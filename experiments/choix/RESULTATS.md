# Campagnes du 25 octobre 2026

La spécification et les seuils ont été committés à `98646b9` avant les mesures.
32 graines appariées × 256 épisodes par bras, sans arrêt opportuniste. IC95 %
de Student sur les 32 moyennes par graine. Résultats complets par graine,
partition et manifeste SHA256 : [resultats-20261025](resultats-20261025).

## Validation du banc

Les selftests natifs et ASan/UBSan passent : capteurs réellement décodés
(dont NEURAL), immédiats, coût unique par fenêtre, délai/paiement, ablation,
contrefactuels, partition équilibrée et reprise complète.

La première campagne `choix-runs/20261025-v1` est conservée. M1 valide ; M2
invalide à cause du contrôleur lookup : les octets de son programme étaient
mis à jour sans ajuster la longueur utilisée par la VM. Correction de cette
longueur, test de régression sur 256 épisodes ; nouvelle campagne complète v2,
avec mêmes graines, seuils et candidat. Le source exact de l'ELF mesuré v4 est
archivé et son hash vérifié contre le manifeste. Aucun résultat v1 M2 n'est
utilisé comme preuve.

Campagne corrigée : `choix-runs/20261025-v2`.

| Mesure | Résultat | IC95 % |
|---|---:|---:|
| M1 témoin informé, décisions correctes | 74,38 % | 73,30–75,45 % |
| M1 témoin informé, gain net hors métabolisme | 13,88 | 13,66–14,09 |
| M1 témoin informé avec indice brouillé | 49,40 % | 48,52–50,29 % |
| M1 candidat general | 24,65 % | 23,72–25,57 % |
| M1 gain candidat − meilleur aveugle | −6,20 | −6,40 à −6,00 |
| M1 chute candidat sous brouillage | 0,24 point | −0,95 à 1,44 point |
| M2 règle câblée, combinaisons réservées | 100 % | 100–100 % |
| M2 lookup des seuls couples vus, sur réservé | 50,27 % | voir JSON |
| M2 candidat, combinaisons réservées | 17,07 % | 16,21–17,93 % |

Tous les critères du **dispositif** M1 et M2 passent. Le coût haut donne net
6,88 à l'informé M1, contre 9,89 au témoin sans consultation : l'observation
coûte bien assez cher pour devenir défavorable.

Le candidat general échoue : il omet exactement 128 décisions sur 256 en M1.
Un score de 24,65 % inclut ces omissions, comptées comme erreurs conformément
au protocole. Le brouillage ne produit pas de chute significative. L'ablation
fait chuter le score en désynchronisant aussi le programme : prise seule,
elle aurait donné une fausse impression de preuve. **Aucun usage de mémoire
utile, aucune composition et aucun raisonnement démontrés pour ce candidat.**
Ce négatif porte sur un génome préexistant non entraîné pour ce nouveau milieu,
pas sur toutes les populations possibles.

## Phase écologique E1

Le [protocole prospectif E1](ECOLOGIE.md) est enregistré séparément, sans changer
les critères M1/M2 : la sélection se fait maintenant par énergie, survie et
reproduction, au lieu d'attendre qu'un fondateur non entraîné soit adapté.
64 fondateurs issus de quatre seeds existantes, aucun contrôle positif injecté.
La recette de lancement est [lancer-e1.sh](lancer-e1.sh).

Les tests écologiques passent en natif et sous ASan/UBSan : 40 cycles continus
produisent exactement les mêmes checkpoints et journaux que 20 cycles, reprise,
puis 20 cycles. Les écritures non validées après checkpoint sont retirées lors
de la reprise. L'export des génomes est en lecture seule sur le checkpoint.

Les journaux de campagne complets compressés restent dans les répertoires des
runs, exclus de git. Les anciens mondes et leurs binaires n'ont pas été modifiés.
