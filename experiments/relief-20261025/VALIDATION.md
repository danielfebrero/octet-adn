# Validation finale — Relief build005

25 octobre2026, essais isolés sous `/tmp`; aucun ancien monde ni production modifié par ces essais.

- ELF statique: `build-005/relief`.
- SHA256 ELF: `c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`.
- SHA256 source `relief.c`: `10ac2a592745b2a83bd657f01961a01a3ff8ee7b8709ce0e4310f74dd3ac080c`.
- Compilation GCC C11 `-O2 -g -Wall -Wextra -Wpedantic -Werror -static`: PASS.
- Selftest et suite CLI build005: PASS.
- Build séparé `build-san-002`, ASan+UBSan, selftest et suite CLI: PASS.
- Revue indépendante et essais supplémentaires: PASS, rapport `/tmp/workflow-relieve-review.md`.

## Invariants et reprise

Quatre graines de selftest (1..4),160ticks chacune, contrôlent la matière à chaque tick. Une trajectoire directe est comparée à une trajectoire sauvée à71ticks puis rechargée: cellules, organismes, état, RNG et compteurs sont identiques.

Tests d'actions: construction avec paiement exact, déconstruction, don, attaque, attaque avec réserve prédatrice presque pleine, mort, reproduction et extinction sans réensemencement. Les deux parités de chaque axe de diffusion sont testées pour éviter des compartiments artificiels.

Tests CLI: double initialisation refusée; dimensions/capacité invalides refusées; corruption checksum et taille refusée; exclu deux écrivains; SIGTERM pendant la vie puis rechargement valide; monde zéro fondateur conserve exactement son checkpoint. Les événements et le snapshot d'un run200ticks sont identiques bit à bit à ceux d'un run73+127ticks, y compris après ajout de queues de journaux non commises. Le reviewer a également comparé80ticks sans cadence à80ticks avec `--tick-ms 2`: état, mesures et événements identiques bit à bit.

## Dimension de production, essai borné1000ticks

Paramètres par défaut: graine20261025,1024×512 cases, capacité65536,8192 fondateurs. Mesure sans cadence pour mesurer le coût réel du calcul (la production demande100ms/tick).

|Mesure|Résultat|
|---|---:|
|Ticks|1000|
|Population finale|65536|
|Naissances depuis initialisation|57451|
|Décès|107|
|Matière initiale et finale|76840404, exactement|
|Temps mur|6,870s|
|CPU utilisateur|6,902s|
|CPU système|0,016s|
|RSS maximum|29016Kio (28,3Mio)|
|Journal événements|12046602octets (11,49Mio)|
|Checkpoint|25166104octets (24,0Mio)|

Ces résultats établissent que le moteur fonctionne avec renouvellement et conserve la matière à cette dimension. **1000ticks ne prouvent pas un équilibre écologique durable.** La population atteint la capacité; les premières générations âgées et l'épuisement/recyclage des ressources restent à observer. Les deux témoins immédiats ne démontrent ni raisonnement ni société.

L'échantillonnage des interactions répétitives réduit le journal par rapport au build003 (64Mio/1000ticks); toutes les naissances/fondateurs/morts demeurent consignées. Le volume du premier millier de ticks contient l'expansion fondatrice et ne prédit pas à lui seul le débit futur. Le superviseur ferme/segmente/gzip les journaux au-delà de32Mio, sans suppression de l'histoire; suivre l'espace disponible.

## Recette de reproduction

```sh
make BUILD=build-NEUF CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -static' test
build-NEUF/relief init /tmp/relief-dimension-NEUVE --seed 20261025
build-NEUF/relief run /tmp/relief-dimension-NEUVE --ticks 1000
build-NEUF/relief inspect /tmp/relief-dimension-NEUVE
```

Conserver les binaires de contrôle et le moteur vivant à des chemins distincts; aucun build ne remplace un ELF existant.
