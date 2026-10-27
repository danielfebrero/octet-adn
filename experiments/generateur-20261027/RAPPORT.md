# Passage-001 — premier rapport d’exécution

Démarrage **27/10/2026,01:47:07UTC**. Relevé **01:48:48UTC**,32lots,**1024ticks** par habitat. Monde laissé vivant : `octet-passage-001`, broker `octet-passage-library`. Prochain regard inscrit **04:50UTC**. Relief et Transmission précédents restent vivants sans interruption. Aucun des34anciens niE1 relancé.

## Fondation

**420individus copiés depuis32mondes** :300terrariums/Raison/jardins,60Relief,60Transmission. **379génomes distincts** initialement. Tous sont descendants dans leurs mondes sources et fondateurs dans Passage. Aucune recette de génome neuf ajoutée. Les doublons génétiques sont des individus distincts, avec mémoire et provenance propres.

Sélection : représentation des sources, diversité génétique et maxima alternés d’âge, énergie, activité, divisions ou conservation selon habitat. «Remarquable» désigne ces critères mesurés, pas une intelligence supposée. Génomes, soma/germen, mémoire, registres et pointeurs restent exacts. Identité, parent local, emplacement et horodatages d’enveloppe sont remappés avec provenance; âge et énergie conservés. Les organismes sources ne sont ni déplacés ni supprimés.

Manifeste `selection-data/selection.json`, SHA256 `ede212fc7a24d8831f2fd4b1a974390dd2124de19d0eb706886d327e97f62325`. Les32checkpoints sources et420records natifs ont été vérifiés.

## Premier relevé

| Habitat | Fondateurs | Vivants | Nouvelles naissances | Décès |
|---|---:|---:|---:|---:|
| Terrarium |300|7279|8512|1533|
| Relief |60|4096|4448|412|
| Transmission |60|60|0|0|
| Total |420|11435|12960|1945|

Le compteur terrarium `births=8812` inclut les300imports; le tableau les retire. Les8512nouvelles naissances moins1533décès expliquent7279vivants. L’archipel s’est étendu de4à32îles vides, sans organisme créé par l’expansion. Relief atteint son plafond4096 : saturation précoce, pas stabilité démontrée.

Transmission compte **60restitutions exactes** du document1174octets, **aucun bonus attribué**, aucune lettre admise, aucune nouvelle requête et aucune naissance sur cet horizon. Les quatre articles65536octets sont hérités du contexte source. Copier exactement ne prouve pas comprendre; les programmes hérités sont à l’origine écrits par nous.

## Chimie et bilans

Les256valeurs ont masse, charge, valence, affinité, catalyse, rétention et dureté tirées par graine. Huit atomes de chaque type sont fournis au milieu initial. Les émissions natives alimentent ce bilan séparé; le surplus est refusé au plafond d’admission sans arrêter la vie.

À1024ticks : **157chaînes présentes**,158compositions cumulées,1dégradation, **36réactions catalysées**,22chaînes ayant servi de catalyseur. Les propriétés des chaînes modifient réellement les réactions; des signaux dérivés du stock et des chaînes deviennent perceptibles aux habitants. Ces fonctions sont conçues par nous. L’exploitation intentionnelle, le savoir et la technologie autonomes ne sont pas démontrés.

Matière Relief **2397624**, égale au bilan initial. Chimie **559949unités**, égales à matière initiale+entrées acceptées, résidu compris. Ces invariants sont contrôlés à chaque lot. Énergie ouverte :36unités prélevées sur la réserve de réactions. Pas de conservation globale revendiquée entre les trois moteurs.

Les événements attestent les sept extensions du tronc : osmo, competition, heritage, memoire, renaissance, lecture, trace. Leur état interne persiste dans les checkpoints. `laws=0` à l’inspection est le nombre de bibliothèques chargées dans ce nouveau processus; elles se chargent au relevé. Ce diagnostic ne signifie pas que ces sept lois sont absentes.

## Ce qui fonctionne

Création par graine, paramètres aléatoires bornés, trois formats natifs sans transcodage, misesàjour àchaud de paramètres/réactions/instructions chimiques, injection d’individus avec provenance, sélection de420successeurs pour un nouveau monde, checkpoints durables et intégrité, refus du double contrôleur, reprise par annulation d’un lot incomplet, extinction sans réensemencement.

Les nouvelles instructions injectables appartiennent à la machine chimique du milieu; elles ne sont pas de nouveaux opcodes dans les VM C. Une telle extension native demande une nouvelle validation.

Tests PASS : imports natifs exacts, inspection sans écriture, refus de corruption et écrasement, reprises natives et état des sept plugins, sélection des successeurs, propriétés256 et bilan chimique. Une intégration temporaire256ticks valide miseàjour de paramètres, nouvelle instruction chimique, injection d’un individu et reçus idempotents. Voir `VALIDATION.txt`, `INTEGRATION.json`, `native/preservation-validation.json` et `selection-data/successors-validation.json`. Les tests n’utilisent aucun ancien monde en exécution.

Production : réseau moteur coupé,1,5CPU/768Mio maximum; broker séparé0,25CPU/96Mio, Grokipedia seulement et seul pont bibliothèque monté. Aucun courrier n’est exécuté. Les deux conteneurs vivent sans redémarrage niOOM au relevé de déploiement. Les séries/événements natifs restent à surveiller pour le stockage.

## Fusion encore partielle

**198fiches spec/descr** :113capacités natives,9règles contrôleur/chimie,1capacité broker et **75contextes historiques non exécutés**. Une capacité disponible ne signifie pas que chaque instruction a été utilisée pendant ce relevé.

Les dispositifs quantiquesQ1/Q2, tâches Choix/Raison et plusieurs bancs de laboratoire restent catalogués avec sources/empreintes; leur sémantique n’est pas rejouée ici. Les créatures Raison gardent leur machine native mais quittent leur ancienne tâche. Les assemblages quantiques requièrent un autre adaptateur et ne font pas partie de cette cohorte. Toutes les constantes historiques ne sont pas encore des variables tirables.

**Le générateur est opérationnel et devient l’entrée de référence des prochains mondes compatibles. La fusion intégrale de toutes les expériences n’est pas achevée.** Aucun savoir, compréhension, société ou technologie autonome n’est démontré par ce premier run.

## Fichiers

`LOIS-PASSAGE-001.md` : toutes les198fiches, spec, descr, statut, source/empreinte, puis les256propriétés. JSON : `LOIS-PASSAGE-001.json`, recette exacte `RECETTE-PASSAGE-001.json`. Compteurs bruts : `FIRST-RUN.json`. Procédures et limites : `README.md`, `SELECTION.md`, `native/README.md`.
