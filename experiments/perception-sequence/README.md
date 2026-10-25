# Perception par séquence : le contre-factuel conserve les actions

## Approche, écrite avant toute mesure

Une organisation n'est reconnue perceptive que si sa séquence exacte d'actions récolte plus d'énergie quand elle reste alignée sur le milieu que lorsqu'on la décale dans le temps.

`metabolic-order` avait un verdict négatif : son marcheur ne conservait que les taux moyens d'action et pouvait mourir alors que l'organisme survivait. Ici le témoin conserve les actions individuelles, leurs arguments, leur nombre et leur ordre cyclique. On demande que **tous** les acteurs comparés survivent. Cette sonde teste l'alignement entre une trajectoire et son milieu ; elle ne prouve ni conscience, ni naissance du bruit, ni perception dans tout environnement.

## Dispositif figé

- Programme C séparé `experiments/perception_sequence.c`, lié au moteur existant ; aucun fichier `src/`, `include/`, `seeds/` modifié, aucune ISA nouvelle.
- Un seul organisme, `max_pop = 1` : `SPLIT` échoue sans effet, sans naissance ni mutation. Monde torique 48 × 20, nourriture initiale de densité 0,35 (quantité 4..14), repousse 4 cases/tick (1..4 unités), énergie initiale 160, les autres paramètres de `cfg_default()` inchangés.
- Horizon **512 ticks**, même monde initial et mêmes tirages de repousse pour la source et ses cinq relectures. L'énergie mangée est `Stats.energy_eaten`, avant plafonnement de l'énergie corporelle ; c'est la quantité récoltée, pas une estimation d'information.
- Trois bras : `forager` (seed embarquée inchangée), bruit uniforme de même longueur, cristal `0x11` de même longueur. Le bruit est tiré d'un RNG distinct, graine `0xA0996A2CD0000000 + s`, pour ne pas déplacer le hasard du monde. Aucun autre génome n'entre dans le compte.
- **32 graines par bras**, `s = 0..31`. Le monde est amorcé par `0x99000000 + s`, commun aux trois bras. Ordre du journal : bras `forager`, `bruit`, `cristal`, puis graines croissantes.
- Pendant le run source, une observation des opcodes enregistre par tick chaque `TURN` (avec sa direction), `STEP`, `EAT`, `ATTACK`, et le premier `EMIT` (avec sa valeur). `SPLIT` est également enregistré pour vérifier son absence d'effet au plafond. Les autres opérations n'ont pas d'effet direct sur le monde. Le relevé est sans effet sur le monde ni sur le RNG.
- Une relecture exacte, décalage 0, applique ces événements au même tick et dans le même ordre. Elle doit retrouver à **chaque tick** le nombre de vivants, position, direction, énergie, nourriture, occupation, RNG et statistiques physiques de la source (âge compris). Les états internes du programme et les compteurs de `STORE` sont exclus de cette égalité : la relecture est un actionneur, pas la VM source. Toute divergence invalide le dispositif.
- Quatre témoins appliquent au tick `t` les événements du tick source `(t + d) mod 512`, avec les décalages fixés **d = 64, 128, 256, 384**. Le décalage est au niveau des ticks ; les événements d'un tick restent dans leur ordre. La somme des événements prescrits sur l'horizon est donc exactement celle de la source. Si un témoin meurt, les événements restants ne sont plus exécutés et la graine échoue ; on ne prétend alors pas conserver le nombre d'événements effectivement exécutés.
- Une source morte ne reçoit plus d'événement ; sa trace est complétée par des ticks vides. Une telle graine échoue. Ni résurrection, ni énergie offerte, ni exclusion des morts du dénominateur.

## Prédiction et seuil de réfutation

Une graine passe si :

1. la source et les **quatre** témoins décalés sont vivants au tick 512 ;
2. la source récolte strictement plus que zéro et au moins **1,10 × la moyenne** des quatre témoins ;
3. la relecture exacte reproduit tous les états physiques attendus.

| bras | volume | seuil pré-enregistré |
|---|---:|---|
| forager | 32 graines | **au moins 24 / 32** passent |
| bruit uniforme | 32 graines | **au plus 1 / 32** passe |
| cristal `0x11` | 32 graines | **au plus 1 / 32** passe |

**Oui seulement si les trois lignes passent et la fidélité de toutes les relectures exactes est vérifiée.** Un seuil raté donne **non** à ce volume. Avec 32 graines, une fréquence réelle de succès de 1/2 atteint 24 succès ou plus avec probabilité 0,0035 ; le volume sépare donc le seuil d'une majorité aléatoire. Aucun second mécanisme, changement d'horizon, de génome, de décalage ou de seuil après mesure.

## Selftest et reproduction

Le selftest vérifie : déterminisme, relecture physique exacte sur `forager` et sur des génomes de bruit, invariance du multiensemble des événements prescrits sous les quatre rotations, et absence de gain d'un actionneur constant sous rotation. Un programme sensoriel fait main est seulement utilisé comme fixture d'intégrité du relevé, jamais dans le compte des graines ni comme résultat de découverte.

Compiler dans un répertoire de build **neuf**, puis :

```sh
CHEMIN_NEUF/perception_sequence selftest
CHEMIN_NEUF/perception_sequence run > /tmp/perception-sequence-0996a2cd.txt
```

Les comptes, les récoltes et le verdict seront ajoutés après le commit de ce pré-enregistrement. Les sorties texte brutes sont lisibles sans JSON ; aucun ELF ou run n'entre dans Git.

## Résultats, après pré-enregistrement `769ac52`

Mesure le 25 octobre 2026 avec l'ELF neuf
`/tmp/octet-0996a2cd-build-A-ps02/perception_sequence`. Selftest vert avant le compte.
Les 96 sources et 480 relectures ont parcouru les 512 ticks ; les morts restent
comptées, leur monde continue sans organisme. Les 96 relectures exactes sont
identiques à la source à chaque tick selon le contrat physique ci-dessus.
Les quatre rotations conservent exactement le nombre d'événements prescrits
pour chaque trace. Source et témoins ne se partagent aucune nourriture.

| bras | graines | source et quatre décalés vivants | passent le gain de 10 % et la survie | seuil | relecture exacte |
|---|---:|---:|---:|---|---:|
| forager | 32 | 32 | **4** | ≥ 24 | 32 / 32 |
| bruit | 32 | 0 | 0 | ≤ 1 | 32 / 32 |
| cristal | 32 | 0 | 0 | ≤ 1 | 32 / 32 |

| bras | énergie récoltée, somme sources | somme des quatre témoins | moyenne par source | moyenne par témoin |
|---|---:|---:|---:|---:|
| forager | 64 160 | 256 870 | 2 005,000 | 2 006,797 |
| bruit | 420 | 1 270 | 13,125 | 9,922 |
| cristal | 0 | 0 | 0 | 0 |

**Verdict : non.** Le seuil pré-enregistré est raté : **4 / 32**, au lieu de
24 / 32. Les quatre graines passantes sont 6, 18, 21 et 30. Le forager récolte
en moyenne 0,090 % de moins que le témoin qui conserve sa séquence complète.
Dans ce dispositif, sa bonne survie n'établit donc pas un gain causé par
l'alignement temporel entre ses actes et le milieu.

Les 32 comparaisons forager ont leurs cinq acteurs vivants : l'échec ne vient
pas d'un témoin mort. Bruit et cristal échouent, mais leur mort ne constitue
pas une preuve indépendante de spécificité du détecteur. La rotation conserve
l'ordre cyclique et les pauses ; elle ne conserve pas le préfixe ni la position
initiale sur le cycle. Cette intervention peut aussi modifier une phase utile
sans relation aux capteurs. Un succès aurait donc indiqué un alignement causal
de trajectoire, et non isolé toute perception. Aucun réglage après les comptes.

Sortie complète : [`results.txt`](results.txt), une ligne par graine, avec
récoltes de chaque témoin et hash physique de la source. Le selftest inclut
également une trace prescrite `EAT` à tous les ticks : toutes ses rotations
ont exactement le même résultat, y compris en cas de mort.
