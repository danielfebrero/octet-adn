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
