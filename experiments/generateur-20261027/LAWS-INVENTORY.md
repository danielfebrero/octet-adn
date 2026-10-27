# Inventaire des lois — Passage, 27octobre2026

`laws.py:generate(seed)` produit198fiches uniques avec `id`, `spec`, `descr`, `params`, chemin et SHA256 de source, statut. Le monde doit conserver la graine, sa recette et le registre produit. `LOIS.md` et `LOIS.json` du monde sont la liste détaillée, incluant les80instructions natives et les256propriétés d'octets. Cet inventaire décrit la portée; ce n'est pas un remplacement du registre.

## Familles exécutables

- **Historique/Terrarium**: VM originale32instructions, RPUSH/RPEEK optionnels, soma/germen et état propre conservés; nourriture, repousse, mouvement, reproduction et mutation. Archipel et migration, canal local/Markov, forces et rôles, effets des octets, briques à recettes, irrigation et espace; stress, attention/chaleur/écho, caractéristiques découvertes, dépense à archipel plein, signaux causaux et saison. Hôte `legacy-terra`, source Terrarium gelée; seules structures auxiliaires neuves sont initialisées pour le nouveau monde.
- **Sept lois tronc**: osmo transfère une nourriture à l'est; compétition transfère de l'île riche à la pauvre; héritage pose marque1; mémoire dépose chaleur après baisse démographique; renaissance ajoute nourriture sous seuil sans créer de vie; lecture compte briques/carrés mixtes; trace marque les carrés mixtes. Leur ABI est chargée par le nouveau Terrarium. Les génomes anciens ne sont pas rejoués par ces plugins.
- **Relief**:16instructions à nibble, corps/réserve/énergie, construction/caches, attaque/parasitisme/don; relief, roches, pentes, humidité, pluie, saisons et diffusion. Matière physique conservée dans sa propre écologie, énergie ouverte. Témoins immédiats perception/mémoire distincts d'une preuve d'intelligence.
- **Transmission**:32instructions, RAM/code modifiables, pointer16bits, huit artefacts/livres, flux binaire, mémoire héritée, conservation exacte récompensée et courrier limité/provenancé. Broker dédié limité à Grokipedia; file de requêtes et service réseau sont des composants distincts. Le statut `native-broker-supported` décrit le dispositif déployé; des requêtes effectivement traitées exigent des traces de broker.
- **Contrôleur**: trois habitats natifs couplés par une chimie d'octets émis et par un retour sensoriel. Aucune reproduction inter-ISA; les créatures gardent leur représentation native. La mise à jour numérique, les réactions déclaratives, les instructions chimiques et les injections d'individus sont journalisées à la frontière de lot.

`native-supported` indique une capacité de l'adaptateur; le mode configuré, les appels et les compteurs établissent les effets réels. Une instruction supportée peut n'être exécutée par aucun organisme.

## Variables tirées à la genèse

| Famille | Domaines de génération |
| --- | --- |
| Historique | repousse2..8; seuil division100..180; mutation0,002..0,02; duplication0,005..0,04; plafond768..1536; budget12..24; registres0/1; germen0/1; vol4..16 |
| Terrarium | osmo1..3; amplitude saison0,1..0,8; période1024..8192; migration256..2048. Forces3/stress/attention/dépense/causales/saison/tronc restent activés pour porter leurs familles. |
| Relief | période saison1024..8192 |
| Transmission | alimentation5..9; diviseur de mutation4..16; âge maximal16384..32768 |
| Chimie | coût réaction1..4; seuil charge2..6; gain catalyse1..4; décroissance128..1024; période réaction2..8; chaîne8..32; budget8..24 |

Les constantes structurelles et les limites de représentation natives ne sont pas tirées arbitrairement: les changer pourrait casser la sémantique des individus. Les bornes validées des mises à jour peuvent être plus larges que les domaines de genèse. Aucun tirage ne garantit la viabilité.

## Physique des256valeurs

Chaque valeur0..255 reçoit: masse1..16, charge−4..4, valence1..4, affinité0..255, catalyse0..4, rétention1..32, dureté1..16. Leur table est distincte de la signification des opcodes natifs.

Par lot, `256//reaction_period` collisions sont tirées. Une paire exige des stocks libres, des valences positives et une somme de charges en valeur absolue≤`bond_threshold`. La probabilité de liaison est `bond_probability × (0,5 + (affinity[a] XOR affinity[b])/510)`. La paire devient une chaîne ordonnée de ses valeurs, sans réécriture. Catalyse, force et rétention viennent de ses deux membres initiaux. La croissance ajoute un octet libre avec probabilité0,02 jusqu'à la longueur limite; ces propriétés de paire initiale restent fixes.

Les règles déclaratives transforment `left+right→product` avec probabilité`rate`. Le résidu de masse va au réservoir; un produit plus lourd exige un réservoir suffisant. Une énergie chimique extérieure augmente de`instruction_budget` par lot, plafond4096; une réaction exige et paie`reaction_cost`. Les catalyseurs augmentent le nombre d'essais, borné par gain et budget. Le compteur d'usage établit la fonction catalytique conçue; il ne prouve pas un savoir ou une technologie.

La décroissance rend tous les octets d'une chaîne libres avec probabilité `1/(decay_period × max(1,retention+strength/4))`. Chaque lot vérifie: masse des atomes libres + chaînes + réservoir = huit fois la somme des256masses initiales + masse acceptée depuis les émissions natives. La chimie est ouverte aux émissions; il n'existe pas de bilan de matière commun aux trois physiques natives.

Le pont sensoriel retourne l'octet dominant et les dernières chaînes. Le Terrarium reçoit son stimulus, Relief son signal, Transmission l'artefact partagé7. Ce couplage est conçu par nous; il peut écraser ces canaux au début de lot. Un assemblage produit n'est jamais exécuté comme code hôte.

## Contextes hérités, non exécutés

Le registre conserve aussi les sources exactes et leur SHA256 pour:

- **Q1/Q2**: positions continues, champs de phase, potentiel réciproque, projections, Langevin/SALR, relaxation, empreintes et filiation des assemblages. Aucun adaptateur fidèle à ces champs n'est présent dans le nouveau moteur à trois ISA.
- **Choix** M1/M2/E1 et les **dix milieux Raison**: mémoire, composition, inversion, observation, patience, chaînage, transfert, signal, autrui, relation; témoins et entrées réservées. Les récompenses propres à ces bancs ne sont pas des lois actives de l'écologie spatiale. E1 reste éteint.
- **Rencontres**: échanges soma/germen, œufs à deux parents, nurseries, rendez-vous, tests comportementaux et sénescence des bras. Leur structure auxiliaire n'est pas importée par le seul transfert d'une créature.
- **Univers à tables**, gardiens et lois locales: tables octet→primitive, dérive, READ_LAW/WRITE_LAW et transmission individuelle de tables; automates innovants. Les tables individuelles ne sont pas remplacées par une ISA standard.
- **Vingt labos**: org-seeds, max-pop, survival, energy-eff, self-rewrite, emit-echo, emit-dialog, predict-walk, predict-stock, predator-prey, fold, antibiotic, commons, pandemic, code-repair, self-evolve, diagnose, general, dialog-mem, dialog-lag2. Les20fiches distinguent leurs fitness historiques.
- Autres sources expérimentales inventoriées: soupes, ensembles, dispersion, ordre/symbiose métaboliques, perception et séquences, registres/transfert, transplantation, incubation, motifs/artefacts, pari phénotypique, détecteurs et sélection par épisodes.

Le statut `historical-context` est explicite: inclure une spécification n'active pas son comportement. L'héritage simultané de toutes les physiques historiques n'est donc pas complet. Il ne faut pas présenter ce registre comme cette réalisation complète.

## Reprise et actualisation

Un écrivain unique utilise un verrou; les ELF sont vérifiés par SHA256. Chaque lot prépare une copie des trois checkpoints, de la chimie, du statut et des stimuli puis marque `READY`. Une reprise restaure ces états si cette marque subsiste; la validation du lot retire durablement la marque. Les effets extérieurs du broker et du courrier et les journaux doivent être réconciliés séparément.

Les patches contiennent identifiant, spécification, description, paramètres validés et empreinte. Les instructions injectables sont des règles chimiques déclaratives; elles n'ajoutent pas automatiquement un opcode C à une ISA. L'injection de créatures exige enregistrements natifs et SHA256. La population nulle demeure extinction, sans réensemencement automatique.

## Portée de l'inventaire

Énumération C/C++ des sources à la racine de `experiments/` et au niveau immédiatement inférieur, plus cœur natif, modules et familles nommées. Exclusion des variantes de compilation, résultats, tests de validation/corrections et observatoires. Les commentaires initiaux donnent les spécifications des familles supplémentaires; le source identifié par empreinte demeure normatif. Ce n'est pas une extraction automatique exhaustive de chaque constante C ni une preuve que chaque constante peut être modifiée sans casser son monde.
