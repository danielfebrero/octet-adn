# Relief — un monde neuf, 25 octobre 2026

Moteur autonome C11, libc et libm seulement; aucun Python ni interpréteur au runtime. Les anciennes écologies ne sont ni importées, ni modifiées. **Un unique monde de production** est prévu. Les petites populations de tests ne sont pas des mondes persistants.

## Dimension et définition de « 10× »

La référence du terrarium historique (`experiments/terrarium.c`, `defaults`) est 4 îles × 64 × 32 = **8 192 cases**. Relief propose **1 024 × 512 = 524 288 cases**, soit **64× cette surface totale** (256× une île), et une capacité de 65 536 organismes. Les 8 192 fondateurs sont répartis sur la carte. La dimension excède donc le facteur 10 spatial demandé. Ce facteur ne décrit ni une dépense CPU multipliée par dix, ni une intelligence mesurée. Le moteur utilise un seul thread; `--tick-ms 100` borne la cadence à environ dix ticks/s, moins quand le calcul dépasse 100 ms. Docker doit également borner les ressources.

La carte n'est pas une grille de niches récompensées. Elle représente des altitudes, parois rocheuses avec passages, pentes infranchissables, réserves minérales, nourriture locale, détritus, humidité, constructions, caches et signaux diffusibles. Les bords sont réfléchissants. Une saison triangulaire affecte la lumière; les fronts de pluie changent tous les 64 ticks. L'humidité s'évapore. Les minéraux et signaux diffusent entre voisins selon quatre matchings couvrant les deux parités de chaque axe. La nourriture se forme avec la lumière **en transférant du minéral**, puis se décompose; constructions et caches retournent également aux détritus.

## Organismes, coûts et causalité

Chaque organisme possède un génome de 64 octets, un pointeur d'exécution, une orientation, huit octets de mémoire, un corps, une réserve de matière, de l'énergie, un âge, un identifiant, son parent et sa lignée fondatrice. Un tick exécute au plus huit instructions; le premier pas est compris dans l'entretien, chaque pas supplémentaire coûte une unité d'énergie. L'entretien coûte `1 + corps/24 + réserve/128 + mémoires_actives/4`, plus un coût de sécheresse hors abri. Le corps peut se réduire en famine, avec retour physique du morceau perdu aux détritus. L'âge maximal est héritable, suivi d'une mortalité probabiliste déterministe depuis la graine.

Les fondateurs reçoivent quatre familles de programmes partiellement viables, dont des variantes exploratoires aléatoires. **Ce n'est pas une émergence de la vie depuis des octets uniformément aléatoires**: la question mesurée est leur évolution dans un milieu matériel. Les programmes ne reçoivent aucun score de coopération, de société, de prédation ou de construction. La sélection résulte uniquement de leurs effets et des coûts. La reproduction exige un corps ≥12, une réserve ≥8, une énergie ≥100 et une case voisine libre; elle coûte20 énergie puis partage corps, réserve et énergie. Quatre octets de mémoire sont hérités, quatre sont remis à zéro. Les mutations modifient des bits (probabilité25% par naissance, avec rares rafales). Les nouveau-nés n'agissent pas dans leur tick de naissance.

Le bas nibble de chaque octet est l'opération; le haut nibble son argument. Les décisions ne consultent que la case courante, la case devant soi, l'organisme courant et sa mémoire. Les identifiants et lignées sont des instruments d'observation et n'accordent aucun avantage.

| Opération | Effet local et coût énergétique |
|---|---|
|0 manger|1 énergie; 1..4 nourriture → réserve et24 énergie par unité, énergie plafonnée|
|1 avancer|2 + corps/16 + construction_destination/8; pas à travers roche/pente forte/occupant|
|2 orienter|1 énergie|
|3 croître|4 énergie et1 réserve →1 corps|
|4 se reproduire|20 énergie puis partage; voir conditions ci-dessus|
|5 construire|6 énergie et1 réserve →1 construction; marque de lignée persistante|
|6 défaire|4 énergie;1 construction → réserve|
|7 déposer|2 énergie;1 réserve → cache commune|
|8 reprendre|2 énergie;1 cache → réserve|
|9 attaquer|8 énergie si plus fort que corps+abri; prélève réserve puis corps de la cible|
|10 parasiter|4 énergie; petit organisme siphonne jusqu'à8 énergie du voisin|
|11 donner|2 énergie plus transfert de4 énergie et1 réserve à un voisin; aucun bonus|
|12 émettre|3 énergie; signal local diffusible et dégradable|
|13 percevoir|mémorise une mesure locale; prix des instructions supplémentaires|
|14 conditionner|saute un octet selon mémoire et seuil; prix des instructions supplémentaires|
|15 attendre|entretien normal|

Les constructions réduisent l'effet de la sécheresse, freinent le déplacement et protègent des attaques. Les caches peuvent être prises par tout occupant ultérieur. La marque physique de lignée survit au constructeur jusqu'à dégradation de la construction: trace héritée par le milieu, sans reconnaissance sociale gratuite. Les comportements échangistes, prédateurs, parasitaires et coopératifs sont possibles; aucun résultat observé n'est automatiquement appelé « société ».

## Invariants, observations et limites

**La matière totale est exactement conservée** entre minéral, nourriture, détritus, construction, cache, corps et réserve. Elle est vérifiée au chargement, à chaque échantillon et avant chaque sauvegarde terminale. Les roches contiennent un stock initial inaccessible. L'énergie est un système ouvert: production chimique depuis la lumière, dissipation par les coûts, plafonds et décès. Elle n'est pas annoncée comme conservée. L'humidité et les signaux ne sont pas de la matière de ce bilan.

`series.tsv` donne les quantités physiques, population, diversité de hashes de génome (collisions possibles), lignées fondatrices vivantes, âge maximal, compteurs cumulatifs de reproduction, mortalité, mouvement, ingestion, construction/déconstruction, dons, attaques, parasitisme, mutation, dépense énergétique et photosynthèse. Les compteurs sont **exhaustifs** et ne sont pas remis à zéro entre échantillons. Les résultats de `build`, `attack`, etc. comptent des opérations; l'absence de compteur ne prouve pas l'absence d'instructions tentées.

`events.tsv` contient **tous** les fondateurs, naissances et décès, avec identifiants/lignée/parent, position, hash et génome complet pour les naissances. Les événements répétitifs `build`, `deposit`, `attack`, `parasite`, `gift` sont **échantillonnés à1/256** par hash déterministe de `(tick,id,kind)`, sans consommer le RNG. Ce fichier n'est donc pas un graphe exhaustif des interactions. Les compteurs dans `series.tsv` restent exhaustifs. Retrouver une chaîne sociale exige une analyse et des témoins supplémentaires.

Les témoins `sensor_witness` et `memory_witness` comparent, sur un échantillon déterministe d'environ1/257 organismes par tick, l'action choisie à une copie de l'organisme dont on neutralise les perceptions immédiates ou la mémoire. Aucune copie n'agit sur le monde et aucun RNG n'est consommé. Ces nombres mesurent uniquement une dépendance immédiate du choix d'instruction: ils ne démontrent ni apprentissage, ni raisonnement, ni avantage adaptatif à long terme. La mémoire neutralisée peut être réécrite par une perception dans la même décision; cette limite est volontaire.

## Construire et vérifier

Toujours un chemin ELF neuf. Le Makefile refuse de réécrire un binaire existant quand la source change. Exemple de production statique:

```sh
cd /workspace/produits/octet-adn/experiments/relief-20261025
make BUILD=build-prod-YYYYMMDD-HHMM CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -static' test
sha256sum build-prod-YYYYMMDD-HHMM/relief relief.c
```

`build-005/relief` est le binaire statique validé de cette livraison. Ne pas utiliser un autre build avec son checkpoint en cours de vie. Les ELF de tests sont distincts. Sanitizers:

```sh
make BUILD=build-san-NEUF CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
```

`selftest` vérifie quatre graines, conservation à chaque tick, déterminisme exact direct/reprise (cellules, organismes, RNG et compteurs), coûts de construction, dons, prédation presque pleine, reproduction, extinction sans réensemencement, et les quatre matchings de diffusion. `tests.sh` vérifie le binaire CLI: paramètres invalides, double initialisation, égalité bit à bit des checkpoints et événements après reprise, réparation des queues non commises, checksum/taille invalides, verrou d'écriture, SIGTERM et reprise, population initiale zéro.

## Commandes et démarrage isolé

`init` exige un **répertoire inexistant** et crée une seule écologie; il refuse tout remplacement. La graine non nulle et tous les paramètres sont inscrits dans le checkpoint. `run` recharge ces paramètres sans possibilité de les changer. `inspect` est une lecture seule d'un checkpoint cohérent. Un autre processus peut inspecter pendant la vie, grâce au renommage atomique.

```sh
ELF=/workspace/produits/octet-adn/experiments/relief-20261025/build-005/relief
RUN=/workspace/produits/octet-adn/experiments/vie-relief-20261025
# À exécuter UNE fois, seulement après décision opérateur d'initialiser:
"$ELF" init "$RUN" --seed 20261025
"$ELF" inspect "$RUN"
# Reprise de ce même état; --ticks est facultatif et borne un essai:
"$ELF" run "$RUN" --tick-ms 100
```

Exemple Docker pour un moteur statique et son superviseur `vivre.sh`, réseau coupé, deux CPU maximum et512Mio de RAM; remplacer IMAGE_LOCALE par une image existante contenant `/bin/sh`, `gzip` et les outils POSIX usuels. Un seul conteneur peut ouvrir l'état en écriture:

```sh
docker run -d --name octet-relief --restart=no \
  --cpus=2 --memory=512m --pids-limit=32 --network=none \
  --read-only --tmpfs /tmp:rw,nosuid,size=16m \
  --mount type=bind,src="$ELF",dst=/relief,readonly \
  --mount type=bind,src="$RUN",dst=/world \
  --mount type=bind,src=/workspace/produits/octet-adn/experiments/relief-20261025/vivre.sh,dst=/vivre.sh,readonly \
  --entrypoint /bin/sh IMAGE_LOCALE /vivre.sh /world /relief
```

`vivre.sh` est le superviseur PID1: il relaie SIGTERM/INT au moteur et attend sa sauvegarde avant de sortir. Il exécute des fenêtres de600ticks à100ms, vérifie l'extinction entre fenêtres et rote les journaux fermés. `docker stop --time 60 octet-relief` laisse finir le tick et sauvegarder. Si le moteur est lancé directement sans ce superviseur, il doit lui-même recevoir SIGTERM (PID1 ou relais explicite). Aucun `--restart=always`: une extinction est une fin normale, pas un incident à ressusciter.

## Sauvegarde, rotation et extinction

`state.ck` contient version, ABI/endianness, tailles, paramètres, RNG, horloge, compteurs, carte et organismes. Le checksum FNV64 détecte les corruptions accidentelles; ce n'est pas une signature cryptographique. À l'ouverture: paramètres, taille exacte du fichier, ABI, checksum, bornes d'organismes/cases, unicité d'occupation, population et conservation sont vérifiés avant exécution. Le format est natif pour cette ABI x86_64; ce n'est pas un format de migration entre versions de moteur. Conserver l'ELF exact et son SHA256 avec l'état.

Sauvegarde: fichier temporaire complet, `fflush`, `fsync`, renommage atomique, `fsync` du répertoire. SIGINT/SIGTERM positionnent un drapeau; le tick en cours s'achève puis journaux et snapshot sont synchronisés. La sauvegarde périodique vaut2 000 ticks (≈200s à cadence100ms). SIGKILL/coupure peut perdre le travail depuis ce point; la reprise enlève les lignes TSV postérieures au tick sauvegardé et les queues incomplètes avant rejeu déterministe. Un arrêt borné écrit un échantillon supplémentaire: la suite de mesures peut donc contenir des ticks non multiples de100 sans modifier le monde. Le verrou `flock` empêche deux écrivains; ne pas retirer `runner.lock`.

Les journaux sont supervisés par `vivre.sh`: après chaque fenêtre de600ticks, le moteur a fermé ses journaux et synchronisé son checkpoint. Les fichiers actifs dépassant32Mio sont **renommés intégralement** sous `histoire/`, leur en-tête est recréé dans un fichier actif neuf, puis le segment fermé est compressé par gzip. Il n'y a donc aucune course entre copie/troncature et écrivain actif, ni perte volontaire d'événements. Aucune archive n'est supprimée automatiquement. Une rotation interrompt seulement le temps réel entre deux ticks et ne modifie ni état ni RNG. Ne pas lancer un autre écrivain pendant cette maintenance; le conteneur superviseur doit rester unique. Surveiller l'espace disque, y compris l'histoire compressée, et conserver les segments avec le checkpoint et l'ELF pour une analyse complète. Ne jamais tronquer, déplacer ou toucher `state.ck` pour roter des journaux. La reprise ne réécrit que les deux journaux actifs, pas les archives compressées.

**Si la population atteint zéro**, `run` termine normalement, sauvegarde et l'annonce. Aucun fondateur n'est recréé; relancer `run` sur ce checkpoint conserve la population zéro et le tick. Garder l'extinction comme résultat. Toute nouvelle graine exigerait une décision explicite et un autre répertoire.

## Mesure de dimension, sans promesse de survie

Un essai borné de développement (build003, avant correctif final de fronts de pluie et échantillonnage) sur la dimension de production a atteint1 000 ticks: population65 536,58 316 naissances,972 décès, matière76 840 404 inchangée. Temps mur7,85s, CPU7,82s, RSS29,2Mio. Il démontre l'exécution et le renouvellement sur cet horizon, pas une stabilité écologique de long terme. Le test final et ses résultats sont consignés séparément dans `VALIDATION.md`. La saturation à la capacité est un résultat à surveiller: comparer ressources, mortalité et renouvellement avant de parler d'équilibre.
