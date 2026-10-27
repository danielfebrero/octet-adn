# Adaptateurs natifs du générateur

Trois sémantiques natives ; `legacy-terra` est l’adaptateur de production du compartiment legacy, `legacy` reste un témoin d’écologie minimale. Aucun génome n'est traduit dans une ISA commune. Les fichiers `frozen/` sont des copies de sources, jamais les sources ou ELF des mondes vivants.

```
make -C native
native/build/legacy-terra init DIR selection-data/legacy.raw SEED
native/build/relief init DIR selection-data/relief.raw SEED
native/build/transmission init DIR selection-data/transmission.raw SEED CONTEXT_STATE_CK
native/build/ISA step DIR N
native/build/ISA inspect DIR
native/build/ISA inject DIR RECORDS_RAW
```

`CONTEXT_STATE_CK` est la copie cohérente archivée du checkpoint Transmission source. Elle fournit le document évaluateur (1174 octets), les bibliothèques et les artefacts partagés. Aucun accès au processus source. Les répertoires du monde doivent être sérialisés par le contrôleur : ces commandes ne prennent pas elles-mêmes de verrou.

## Import

Entrée : concaténation de structs natives exactes `Creature` ou `Agent`, dont ABI et tailles sont consignées dans le manifeste de sélection. Les génomes, code, mémoire, sortie, PC, registres, germen, fenêtres et compteurs individuels sont copiés sans réécriture. L'âge biologique et l'énergie sont conservés. Enveloppe remappée : identifiants locaux, parent local initial, lignée locale pour Relief/Transmission, position locale et date de naissance relative. Les trois horodatages de courrier/récompense/requête de Transmission sont réinitialisés à zéro. La filiation historique reste dans `selection.json` ; `parent=0` local ne signifie pas fondateur historique.

La matière corporelle des arrivants Relief est comptée comme import physique explicite : `initial_matter` devient la matière totale après chaque injection. Elle est conservée exactement pendant les pas sans injection.

## Paramètres vivants

`params.tsv` : `clé valeur` numérique par ligne, relu avant chaque batch. L'injection de lois est donc effective à la frontière suivante de batch, sans réinitialisation de population.

- legacy : food_regrow [0,10000], split_energy [1,1000000], mut_rate et dup_rate [0,1], max_pop [1,4096], ops_per_tick [1,4096], regs et germline [0,1], attack_steal [0,1000000].
- Relief : season [16,10000000].
- Transmission : native_eat_gain [1,64], native_mutation_divisor [1,100000], native_max_age [256,1000000].

Seuls ces paramètres modifient les lois natives. Les autres extensions du générateur appartiennent à la couche environnementale Python.

## Pont d'octets

`stimulus.bin` est fourni par le contrôleur avant un batch. Son premier octet devient le stimulus sensible legacy et le signal des cellules Relief. Transmission copie jusqu'à 4096 octets dans l'artefact partagé 7, lisible par son ISA. Ce pont ne fournit aucune récompense énergétique automatique ni matière gratuite.

`emissions.bin` contient les octets effectivement émis pendant le batch terminé : `EMIT` legacy et Transmission, ou octet bas du signal Relief après une action signal réussie. Maximum 65536 octets par batch, surplus non exporté. Ce fichier ne doit pas être interprété comme un programme hôte.

## Terrarium natif de production

`legacy-terra` exécute le terrarium complet gelé, avec l’archipel, canal social, migration, forces3/briques/recettes/espace, stress, attention, traits, dépense, causales, saison et chargement de lois `.so` activés. Quatre îles initiales ; 300 sélectionnés répartis sans changement de génome, registres ou mémoire. L’expansion ouvre des îles vides sans fabriquer d’organismes.

Copier les sept `build/plugins/*.so` dans `DIR/lois/` avant les premiers pas. Le relevé toutes les100ticks charge puis applique osmo, competition, heritage, memoire, renaissance, lecture, trace. Les plugins gelés exposent un accès à leur petit état interne ; les7x3entiers correspondants sont conservés dans le checkpoint du terrarium entre commandes. Mémoire conserve donc réellement la population du relevé précédent. Les marques physiques `mark[][]`, absentes du checkpoint historique original, sont également persistées dans cette extension propre au générateur.

Le checkpoint reprend le payload Terra suivi de cette extension et d’un FNV64 couvrant la totalité. Écriture temporaire, fsync, renommage et fsync du répertoire. Toute corruption est rejetée. Le champ diagnostic `laws` est le nombre de bibliothèques actuellement chargées dans le processus ; après une simple inspection/reprise il vaut0 jusqu’au prochain relevé. Cela ne désactive pas le chargement configuré des sept plugins.

Paramètres supplémentaires relus à chaque batch : `regrow`[0,10000], `mut`/`dup`[0,1] (alias), `forces`[0,3], `stress`/`attention`[0,1], `osmo`[0,32], `season_amp`[0,1], `season_period`[1,10000000], `migrate`[0,10000000], `depense`/`causales`/`saison`/`tronc`[0,1]. Les paramètres core déjà documentés sont appliqués à chaque île.

Dans cet adaptateur le pont d’émissions exporte le dernier octet émis de chaque survivant à chaque tick. Les émissions multiples d’un individu pendant le même tick ne sont pas toutes exportées. Le stimulus externe ajuste l’environnement de Markov au début du batch ; l’environnement natif peut ensuite évoluer normalement.

Les protocoles auxiliaires raison/choix/quantique et jardins restent des contextes historiques spécifiques, non des protocoles expérimentaux identiques rejoués ici. Les créatures sélectionnées conservent tout leur état `Creature` natif ; aucune adaptation en un génome standard n’est effectuée.

Les sources gelées Relief et Transmission comportent uniquement les points d'export des émissions et les trois paramètres Transmission décrits ; le reste de leur VM est inchangé. Aucun ancien monde n'est relancé.

## Vérifications

`python3 native/test_native.py` importe les 420 records sélectionnés sur des copies temporaires, vérifie les trois populations initiales, contrôle que l'inspection ne modifie pas les checkpoints, compare les checkpoints d'un batch30 à deux batch15 (reprise déterministe), rejette les corruptions, puis teste l'injection d'un record. Le test ne touche jamais les mondes en cours.

`python3 native/test_preservation.py` vérifie en plus tous les octets des records après import (seule l’enveloppe tracée est exclue), refuse init sur un monde existant, compare les états natifs d’une exécution200ticks et100+100 avec les sept plugins et leurs états, contrôle les trois intégrités, et mesure les latences init/inspect/step32. Les frontières de comparaison Terra sont alignées sur ses relevés ; les effets d’expansion sont évalués aux frontières du batch, conformément au contrôleur. L’inspection ne modifie jamais de checkpoint. Résultats : `preservation-validation.json`.
