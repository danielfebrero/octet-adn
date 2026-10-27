# Sélection native des 420 migrants

Le programme `selection.py` lit les checkpoints en lecture seule. Il compile des extracteurs qui incluent les sources C des moteurs et appellent uniquement leurs chargeurs : aucune itération du monde, aucun programme de créature, aucune initialisation biologique. Les snapshots sont des copies intégrales de générations de checkpoints atomiques. Les archives restent arrêtées et les deux mondes courants restent intacts.

## Périmètre et choix

- 300 descendants des moteurs Octet historiques : 10 dans chacun des 18 terrariums, des 10 arènes de raisonnement et des 2 jardins.
- 60 descendants de Relief et 60 descendants de Transmission.
- 420 individus authentiques, tous avec un parent documenté ; aucune fondation synthétique, aucun génome construit, aucun réensemencement d'E1.
- 379 couples ISA–génome distincts dans cette sélection. Les génotypes répétés appartiennent à des individus distincts ; ils sont conservés lorsque la diversité disponible dans leur provenance est limitée.

« Remarquable » désigne ici une politique explicite de conservation, pas une preuve d'intelligence : les maxima alternés d'âge, énergie, écritures utiles, divisions, pas, écritures, alimentation et réserves sont privilégiés avec une première passe de diversité génomique. Transmission ajoute les dernières récompenses, lettres et demandes de bibliothèque. Ces métriques ne constituent pas un classement de compréhension ou d'adaptation entre moteurs. Chaque individu porte son motif de sélection et sa valeur mesurée. Une quota par provenance évite qu'un moteur numériquement dominant efface les expériences plus petites.

Pour les terrariums et les arènes, l'extracteur présélectionne les 32 maxima de huit métriques avant la sélection finale. Pour Relief et Transmission, la sélection examine l'intégralité des vivants de la copie. Les autres individus restent dans les snapshots : une sélection ultérieure peut reprendre d'autres critères sans réinventer leurs octets.

## Conservation des données

`selection-data/selection.json` contient l'identifiant originel et celui de sa provenance, le parent, les champs de machine pertinents, le génome et la mémoire séparés, ainsi que l'enregistrement C natif entier en base64, son SHA-256 et son offset exact dans le checkpoint. Les champs sans équivalent entre machines ne sont pas inventés. Le cœur historique n'a pas une RAM séparée : son ADN mutable, ses registres, son germen, ses fenêtres, ses marques dirty et ses autres champs sont tous dans l'enregistrement natif.

`legacy.raw`, `relief.raw` et `transmission.raw` concatènent les enregistrements natifs dans l'ordre du manifeste pour chaque ISA : respectivement 300 × 2360, 60 × 128 et 60 × 8784 octets, ABI Linux x86-64 des sources locales. Le consommateur doit vérifier ces tailles et utiliser l'ISA native correspondante. Il ne faut ni tronquer les programmes ni les passer dans une ISA commune. Relocaliser un individu dans un monde neuf est un effet d'accueil à documenter séparément, pas un changement de sa fiche source.

Le checkpoint Transmission complet conserve également le texte confié, la bibliothèque, l'espace partagé et les sorties partielles. Les checkpoints Raison conservent les `Ind` complets, dont les récompenses différées et le pool de naissance, même si l'interface migrante exécute leur `Creature` native dans un autre milieu. Les contextes d'origine ne sont pas une promesse de reproduire la même trajectoire dans le monde successeur.

## Limites explicites

Les quatre mondes quantiques ont des états et des dynamiques distincts de ces trois ISA. Ils sont inventoriés mais ne sont pas transformés artificiellement en génomes Octet ; leurs organismes nécessitent leurs propres adaptateurs quantiques. Leur omission de cette première cohorte doit être annoncée. E1 est éteint et n'offre aucun vivant à sélectionner : il n'est pas ressuscité.

Les lois des contextes sources, notamment les lois de rencontre, d'épisodes et les modules dynamiques, ne sont pas contenues intégralement dans le seul corps d'une créature. Conserver ses octets n'équivaut pas à recréer toutes ses relations et toutes ses conditions originales. Les adaptateurs et le manifeste des lois du nouveau monde font foi sur l'exécution effective.

## Validation

Chaque enregistrement sélectionné est recherché octet pour octet dans le snapshot source ; l'offset est enregistré. Le script vérifie exactement 420 individus. Les contrôles supplémentaires comparent les SHA des snapshots, les tailles et empreintes de chaque enregistrement, les blobs de génome/RAM et les concaténations livrées. Les snapshots volumineux et les exécutables d'extraction sont des artefacts de preuve locaux, pas des fichiers à ajouter implicitement dans Git.

## Emmener les descendants vers le monde suivant

`python3 select_successors.py CHEMIN_DU_MONDE --count 420 --output prochaine-cohorte.json`

Cette commande utilise uniquement `export` et `inspect` des adaptateurs natifs sur des copies de checkpoints du monde généré. Elle n'initialise aucun monde et ne fait avancer aucun tick. Les exécutables sont choisis selon les SHA du manifeste de génération, pour employer le bon chargeur historique ou Terra.

La cohorte n'impose pas la répartition initiale 300/60/60. Les quotas suivent les effectifs vivants, avec au moins un individu de chaque habitat encore vivant lorsque le nombre demandé le permet. Un habitat éteint n'est pas repeuplé. S'il reste moins de 420 organismes, la commande échoue en indiquant l'effectif disponible ; un autre `--count` explicite permet de poursuivre avec un effectif inférieur, sans fabriquer de fondateurs.

`prochaine-cohorte.json` suit le même schéma de créatures que la première sélection et fournit `transmission_context`, chemin du checkpoint intégral à transmettre à l'initialisation suivante. Son dossier `.sources` contient les snapshots et les exports natifs. Les checkpoints des habitats sont atomiques individuellement ; leurs ticks peuvent différer si le contrôleur continue pendant la relève. Le manifeste annonce cette limite.

Les identifiants biologiques historiques peuvent se répéter entre îles Terra : l'identité de provenance utilise donc le checkpoint, l'offset natif et l'index d'export ; le champ `source_id` conserve l'identifiant biologique sans prétendre qu'il est global. Les octets sélectionnés sont vérifiés à leur emplacement exact dans la copie source. Aucune conversion d'ISA n'est effectuée.

Le contrôle d'intégration `selection-data/successors-validation.json` atteste l'extraction de 420 migrants d'un monde neuf de contrôle, les identifiants uniques malgré les identifiants locaux répétés, l'absence de changement des checkpoints sources et le refus d'inventer le 421e vivant absent. Ce monde de contrôle est temporaire et ne remplace aucun monde vivant.
