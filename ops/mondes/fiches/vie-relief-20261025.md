# Relief

Seul monde courant depuis le GO du 25 octobre 2026. Les 34 anciens restent archivés ; E1 reste éteint. Population nulle = extinction sans réensemencement ; reprise sur son checkpoint avec ELF005 exact uniquement.

## Faits remarquables

### 26 octobre 2026, 15:18 UTC — intérieur des ticks 693 000 à 693 600

Copie en mémoire du checkpoint courant chargée par `load`, sans `init`, `run`, `save`, verrou ni journal ouvert. Instrumentation d'une copie du source sous /tmp. Aucun changement du moteur vivant. Au prochain checkpoint réel : comparaison de tout World sans pointeurs, de toutes les cellules et de tous les agents octet par octet. **FULL_STATE_VERIFY tick=693600 equal=1**.

| Mesure dans la fenêtre | Valeur |
| --- | ---: |
| Population aux deux extrémités | 65 536 |
| Naissances / morts | 11 069 / 11 069 |
| Instructions de reproduction présentées à action() | 10 256 625 |
| Reproductions effectives | 11 069 (0,108 %) |
| Dons effectifs | 118 870 |
| Actions de bénéficiaires après un don dans le même tick | 59 159 |
| Décodages | 39 310 573 |
| Décodages de plusieurs instructions | 3 095 743 (7,88 %) |
| Décodages modifiant la mémoire | 593 986 |
| Instruction sélectionnée différente sans perceptions | 10 544 |
| Instruction sélectionnée différente avec mémoire initiale nulle | 169 509 |
| Matière aux deux extrémités | 76 840 404 = initiale |

**Enchaînements.** Au tick 693 001, 16 792 253 reçoit un don puis présente une instruction de don (op11) : réception puis tentative de redonner, sans preuve que ce second don aboutit. 16 778 731 reçoit puis présente manger (op0) ; 16 818 541 reçoit puis tourner (op2). Les 59 159 comptent des passages à action(), pas des individus distincts ni un bénéfice causal du don.

**Ordre du tick.** Mise à jour du milieu, index de départ pseudoaléatoire, parcours cyclique séquentiel des emplacements. Entretien et mortalité avant décodage de huit instructions au plus, puis une action physique au plus. Les conséquences sont immédiatement visibles aux suivants. Un nouveau-né n'agit pas pendant son tick de naissance. Population pleine aux sauvegardes ne signifie pas absence de places temporairement libérées.

**Limites.** Rejeu déterministe validé sur le vivant, pas lecture de la mémoire de son processus ni alignement sur chaque instant mural. Les tentatives peuvent échouer pour de nombreuses raisons ; ne pas attribuer tous les refus reproductifs au plafond. Les contrefactuels mesurent le changement d'instruction sélectionnée, sans prouver adaptation, raisonnement ou coopération. Les décodages peuvent ensuite être refusés faute d'énergie. Les champs reads/branches de la trace sont une reconstruction auxiliaire approximative utilisant la mémoire finale : ils ne sont pas interprétés. effects indique un changement local/corporel ou des compteurs naissance/don, pas le succès biologique de chaque opcode.

**Preuves.** [Source instrumentée autonome](../../../experiments/relief-observatoire/observation-20261026-intratick.c), [trace intégrale et vérification](../../../experiments/relief-observatoire/observation-20261026-intratick.txt). Compilation `cc -O2 -std=c11 -Wall -Wextra -o /tmp/relief-observe observation-20261026-intratick.c -lm`, exécution sur le répertoire du monde en lecture seule, arrêt après comparaison du checkpoint suivant. Coût ponctuel sur un cœur.

### État et stockage au même regard

ELF005 SHA-256 c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480 ; source 10ac2a592745b2a83bd657f01961a01a3ff8ee7b8709ce0e4310f74dd3ac080c. octet-relief démarré le 25/10 à 19:56:08 UTC, zéro redémarrage, aucun OOM au regard. Monde 1024 × 512, seed 20261025.

STOCKAGE.txt du 26/10 à 15:07:05 UTC : statut ok, 279,2 Go libres (décimaux), histoire 1,701 Go, 128 gz, zéro xz ; projection 365 jours à 0,766 Go/jour. df au regard : 260 Gio libres. Aucun segment âgé de 24 h, donc recompression pas encore commencée. stockage.log absent et journal Docker du service vide : cohérent avec zéro segment recompressé, pas preuve d'un contrôle détaillé. Disque partagé, marge projetée non garantie.

[Exploitation](../../../experiments/relief-20261025/EXPLOITATION.md) · [Observatoire](../../../experiments/relief-observatoire/README.md) · [Index](../FAITS-REMARQUABLES.md#relief)
