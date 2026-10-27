# Générateur de mondes successifs

Entrée de référence pour les prochains mondes octet-adn. Un tirage reproductible des paramètres et une sélection d'individus existants remplacent les fondations de neuf. Les anciens programmes restent des sources et des adaptateurs; ne pas les relancer pour créer un monde à côté de ce générateur.

## Ce qui est implanté

Un monde est un archipel hétérogène: terrarium natif complet, Relief natif et Transmission native partagent une chimie et des ponts sensoriels. Les organismes conservent exactement soma/germen, code, mémoire, registres et pointeur au passage. Le nouvel emplacement et l'identité locale changent; le manifeste conserve les identités précédentes. L'âge et l'énergie ne sont pas remis à neuf. Les descendants du nouveau monde sont ensuite sélectionnables pour le suivant.

Les forces, causes, briques, stress, attention, traits, dépense, signaux causaux et saisons du terrarium sont activés. Les sept lois du tronc sont chargées dans son nouvel espace; leur mémoire interne est sauvegardée. Relief conserve son bilan de matière, son climat et ses16opérations. Transmission conserve code/RAM, restitution, bibliothèque et courrier. La bibliothèque déjà acquise est transférée dans le contexte; le broker séparé n'accède qu'à Grokipedia.

Chaque valeur0..255 a masse, charge, valence, affinité, catalyse, rétention et dureté. Les octets émis alimentent un inventaire commun; des collisions forment des chaînes, qui peuvent croître, se dégrader et catalyser des réactions. Le bilan de masse de cette chimie est vérifié, séparément de celui de Relief: les émissions sont une entrée extérieure. Des atomes initiaux sont fournis au milieu. Une chaîne fonctionnelle n'est pas une preuve de découverte sémantique: catalyse et codage sensoriel sont des règles écrites par nous. Aucun savoir ni technologie autonome n'est annoncé.

## Limites de la fusion

Ce programme ne réunit pas toute la sémantique de tous les anciens laboratoires. Les dispositifs quantiques, les tâches de récompense Raison/Choix et plusieurs bancs spécialisés sont conservés dans le catalogue, avec leurs sources et empreintes, mais ne sont pas exécutés ici. Les créatures Raison utilisent leur ISA native dans un milieu différent, sans conserver le problème de laboratoire. Les corps quantiques nécessitent un autre adaptateur et ne figurent pas parmi les420. Les habitats ne se croisent pas génétiquement entre ISA.

Toutes les constantes historiques ne sont pas encore des variables tirables: le générateur tire les paramètres exposés et la physique des256octets. Les indicateurs nécessaires à l'héritage des familles sont activés; les variables de chaque ancienne loi ne sont pas toutes exposées. L'exclusivité du générateur est donc la convention retenue pour les futurs mondes compatibles, avec extension du registre pour les autres formats.

## Commandes

Depuis ce répertoire, construire les adaptateurs une fois, à un chemin neuf avant production:

```sh
make -C native
python3 -m unittest -v test_generator
python3 generator.py create ../vie-passage-001-20261027 --selection selection-data/selection.json --seed 202610270138 --transmission-context selection-data/snapshots/experiments__vie-transmission-20261026/state.ck
python3 generator.py run ../vie-passage-001-20261027
python3 generator.py inspect ../vie-passage-001-20261027
python3 generator.py laws ../vie-passage-001-20261027
```

Ne jamais reconstruire les ELF d'un monde en cours: le manifeste fixe leur empreinte et leur chemin. `create` refuse un répertoire existant; aucun superviseur ne réensemence après extinction. Les originaux Relief et Transmission et les34mondes archivés restent distincts et intacts.

### Mises à jour et injections en cours de vie

```sh
python3 generator.py update WORLD example-update.json
python3 generator.py inject WORLD selection-avec-provenance.json
```

Une mise à jour est un JSON déclaratif borné: `native_params`, nouvelles `reactions`, nouvelles `instructions` chimiques, `note`. Les nouvelles instructions opèrent dans la machine chimique du milieu; ce ne sont pas de nouveaux opcodes C dans les VM natives. Une extension de VM native nécessite un nouvel adaptateur, une validation et un monde/version distinct ou une migration explicite. Les créatures ne peuvent jamais exécuter du code hôte ni installer des `.so`.

Les changements sont appliqués à une frontière de lot et conservés avec SHA256, version et numéro de lot. Les mêmes demandes ne sont pas réappliquées. L'injection garde les octets natifs et leur provenance, refuse les dépassements de capacité et ne transforme pas l'ISA.

### Monde suivant

```sh
python3 select_successors.py WORLD --count 420 --output /chemin/cohorte-suivante.json
python3 generator.py create NOUVEAU_WORLD --selection /chemin/cohorte-suivante.json --seed NOUVELLE_GRAINE --transmission-context /chemin/cohorte-suivante.json.sources/transmission/state.ck
```

La sélection ne tue ni ne déplace l'individu source. Elle transfère une copie exacte de son état dans le nouveau monde. S'il reste moins de420vivants, elle refuse plutôt que de fabriquer des fondateurs. Le choix des quotas suit les populations vivantes; aucun format de créature n'est normalisé.

## Vie, crash et stockage

Le contrôleur avance par lots32ticks puis attend jusqu'à3,2secondes par lot, hors calcul lent. Chaque habitat sauvegarde son état. Le contrôleur garde une sauvegarde de transaction des trois checkpoints, du stimulus, du curseur courrier et des états chimie/rapport avant chaque lot. Une transaction incomplète est annulée à la reprise, puis rejouée. Les entrées réseau et fichiers externes ne constituent pas une transaction distribuée: une requête déjà servie n'est pas annulée. Les queues TSV natives sont réparées à la reprise par leurs mécanismes d'origine.

Un verrou mondial interdit deux contrôleurs. Les inspections sont en lecture seule. Les rapports utilisent un anneau de256fichiers, les artefacts1024places et les chaînes une longueur bornée. Les inventaires libres sont limités à l'entrée65536atomes; la dégradation peut libérer en plus les atomes des chaînes existantes. Le courrier Transmission est borné64Mio/16384lettres. Les séries et événements natifs doivent être surveillés/archivés sans troncature du vivant. Les snapshots sources sont des preuves locales horsGit.

Les règles aléatoires, la fondation et les entrées sont traçables; le calendrier des threads et le réseau empêchent de promettre une reproduction bitàbit de tout le monde fédéré. Les tests vérifient les invariants ciblés et les reprises natives. Les dates du rapport permettent de comparer uniquement ce qui a effectivement vécu.
