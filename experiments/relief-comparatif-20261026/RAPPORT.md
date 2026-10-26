# Relief : les caches, la survie et les descendants

Rapport final du 26 octobre 2026 — mission `b8e25e22-c4a1-4e91-814f-a8dd2ce11876`.

**Six essais réellement exécutés, deux états de départ, aucune modification du vivant.** Convertir localement environ un quart des caches en nourriture améliore fortement la survie courte, mais ralentit le remplacement sous plafond et concentre davantage la descendance. Égaliser les quantités de cache entre ceux qui y accèdent déjà a des effets agrégés beaucoup plus petits dans ce protocole. Cela distingue deux interventions concrètes ; cela ne prouve pas une loi universelle sur la matière ou la propriété.

## Résultats principaux, à 8 192 ticks

Chaque bras finit avec 65 536 vivants. Dans chaque trajectoire, naissances et décès cumulés sont égaux. Les **racines** sont les 65 536 individus présents au départ, chacun suivi séparément par tous les liens parent/enfant après le départ. Elles se distinguent des **lignées fondatrices**, bien plus anciennes.

| Fenêtre | Bras | Naissances = décès | Écart témoin | Racines avec descendants vivants | Lignées fondatrices | Génomes exacts |
| --- | --- | --- | --- | --- | --- | --- |
| F1 | Témoin | 249 956 | — | 11 110 | 25 | 28 139 |
| F1 | Déblocage A | 164 329 | -34,26 % | 9 512 | 26 | 26 062 |
| F1 | Redistribution B | 246 754 | -1,28 % | 11 142 | 26 | 27 768 |
| F2 | Témoin | 252 550 | — | 11 112 | 25 | 27 898 |
| F2 | Déblocage A | 164 851 | -34,73 % | 9 547 | 26 | 26 113 |
| F2 | Redistribution B | 249 891 | -1,05 % | 11 111 | 25 | 27 804 |

Le déblocage réduit le renouvellement réalisé de 34,26 % et 34,73 % . Il réduit aussi les racines encore représentées dans la population finale de 14,38 % et 14,08 % . **Ce n'est pas une mesure de fécondité intrinsèque** : sous un plafond, une meilleure survie laisse moins de places vacantes. Les causes des décès et les refus de reproduction ne sont pas instrumentés ici.

La redistribution réduit les naissances de 1,28 % et 1,05 % . Son avantage apparent sur le nombre de racines représentées ne se retrouve pas :+32 dans F1,−1 dans F2. Il ne faut pas transformer cet écart en bénéfice général pour la descendance.

## Survivre, se reproduire, laisser une descendance

| Fenêtre | Bras | Initiaux vivants à 600 | Initiaux vivants à 4096 | Initiaux avec descendance à 8192 | Lignées effectives à 8192 |
| --- | --- | --- | --- | --- | --- |
| F1 | Témoin | 65,52 % | 5,50 % | 16,95 % | 12,525 |
| F1 | Déblocage A | 91,82 % | 8,58 % | 14,51 % | 11,146 |
| F1 | Redistribution B | 65,43 % | 5,44 % | 17,00 % | 12,670 |
| F2 | Témoin | 75,87 % | 7,13 % | 16,96 % | 12,532 |
| F2 | Déblocage A | 92,11 % | 10,42 % | 14,57 % | 11,106 |
| F2 | Redistribution B | 75,92 % | 7,16 % | 16,95 % | 12,552 |

À 8192, **aucun individu initial ne survit dans aucun bras**. Chaque individu final appartient donc à la descendance prospective d'une racine initiale. La moyenne d'un descendant vivant par racine est mécaniquement 65 536/65 536=1 ; elle ne mesure aucun progrès.

Dans les six bras, la médiane des descendants vivants par racine est zéro. Dans F1, les 1 % de racines les plus représentées portent 21,58 % des descendants témoins,22,78 % avec A et 20,93 % avec B ; dans F2 :21,15 % ,23,31 % ,21,15 % . Le Gini de cette descendance passe de 0,9076 à 0,9199 avec A dans F1, de 0,9062 à 0,9208 dans F2 : la représentation des ancêtres devient plus concentrée.

Le nombre brut de lignées et leur répartition racontent des choses différentes. **A conserve 26 lignées contre 25 chez les témoins**, mais leur nombre effectif de Shannon baisse d'environ 11 % et le nombre de génomes exacts baisse de 7,38 % /6,40 % . Le nombre effectif résume l'équilibre des parts : plusieurs lignées minuscules comptent moins qu'autant de lignées équilibrées. Les génomes sont comparés sur leurs 64 octets, et les comptes par hash sont conservés séparément ; aucune collision observée à ces relevés.

Les sorties `results/windowN-lineage-results.tsv` contiennent pour chaque lignée : effectifs et parts, naissances/morts, survie de la cohorte initiale, enfants directs, descendants vivants, diversité et caches accessibles normalisés par tête. Les cohortes individuelles complètes restent dans `data/windowN/cohort-results.tsv`.

## Qui gagne des parts ?

Avec A, la lignée 905 perd 2392 individus par rapport au témoin dans F1 (−3,65 points de population), et 2299 dans F2 (−3,51 points). La 3467 gagne 1910 puis 2229 individus (+2,91 /+3,40 points). Ces différences sont des réponses à cette intervention dans ces états, pas des aptitudes générales démontrées.

Avec B, la composition change beaucoup moins : distance totale entre les parts des lignées et celles du témoin 1,25 % /0,98 % , contre 10,12 % /11,35 % pour A. Cette distance vaut la moitié de la somme des écarts absolus de parts. Les gagnants/perdants de B sont souvent instables entre fenêtres ; les petites lignées doivent être lues avec leur dénominateur. Par exemple, la 3921 n'avait que 7 individus dans F1 : elle disparaît du témoin, reste sous B, alors que 209(8 individus initiaux) disparaît du témoin et de B mais reste sous A. Ces cas ne fondent pas à eux seuls une conclusion générale.

## Des interventions définies et vérifiées avant les essais

**Il n'existe pas de droit de propriété des caches dans ELF005.** Toute cache peut être retirée par un occupant ultérieur. `Cell.owner` conserve uniquement une trace du constructeur ; ce champ n'est jamais consulté pour autoriser un retrait. « Détenteur » signifie donc ici occupant initial d'une case ayant une cache positive.

- Témoin : fichier initial inchangé, octet pour octet.
- A : sur chaque case, `floor(cache/4)` est transféré du cache vers la nourriture de cette même case. Positions, génomes, inventaires, traces de construction et autres champs restent identiques. La somme locale nourriture+cache reste exacte et les caches positives restent positives.
- B : seules les cases initialement occupées à cache positive participent. Dans chacune des 256 phases de décomposition (`position modulo256`), les quantités sont égalisées au quotient entier, puis le reste est attribué par index de case croissant. Quantité globale, quantité sous occupants, support positif et totaux par phase sont conservés. Les cases inoccupées et les occupants sans cache positive ne reçoivent rien.

| Fenêtre | A : unités cache→nourriture | Part des caches | B : unités réaffectées | Cache sous occupants, inchangé B | Gini par occupant, B | Écart des moyennes par lignée, B¹ |
| --- | --- | --- | --- | --- | --- | --- |
| F1 | 16 577 217 | 24,749 % | 4 025 726 | 8 972 825 | 0,6350 → 0,1857 | 0,1946 → 0,0526 |
| F2 | 16 587 234 | 24,750 % | 3 958 262 | 8 831 983 | 0,6368 → 0,1896 | 0,2038 → 0,0519 |

¹Coefficient de variation des moyennes de cache par tête de chaque lignée, pondéré par son effectif. Le Gini par occupant inclut aussi les occupants sans cache. Le nombre de détenteurs positifs reste 60543 dans F1 et 60004 dans F2. Le Gini non pondéré des moyennes par lignée baisse également :0,1627→0,0593 et 0,1516→0,0423. L'égalisation a donc effectivement modifié la concentration ; elle ne s'est pas limitée à réétiqueter des champs inertes.

**Limite d'interprétation de A :** la nourriture produit de l'énergie lorsqu'elle est mangée, devient perceptible par les capteurs, se décompose au rythme 1/64 plutôt que 1/256 et peut modifier la production solaire locale au seuil 96. Le stock de matière et l'énergie ne sont pas la même variable ; l'énergie du modèle est ouverte. A teste une conversion locale en matière comestible, avec ces conséquences, pas une injection de matière ni un changement pur de quantité totale.

**Limite de B :** égalisation ponctuelle entre les accès positifs existants, pas ouverture de l'accès à tous ni règle permanente. Dans F1, seulement 8,97 millions sur 66,98 millions d'unités de cache participent au pool sous occupants. Le Gini par occupant de B remonte à 0,5772 /0,5748 à 8192(témoins 0,6415 /0,6366) avec les déplacements et échanges. L'égalité initiale ne persiste pas intégralement.

Les contrastes sont A−témoin et B−témoin. Les doses et mécanismes diffèrent ; leur différence n'est pas une décomposition universelle disponibilité/propriété. Aucun effet combiné A×B n'est revendiqué et aucun bras combiné n'a été ajouté.

## Matière, interactions et plafond

Les 76840404 unités de matière sont conservées exactement à chaque export et contrôle du moteur. À 8192, les réserves physiques diffèrent :

| Fenêtre | Bras | Caches | Nourriture | Corps+inventaires | Énergie² | Dons | Attaques | Parasitismes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| F1 | Témoin | 67 134 987 | 219 862 | 3 573 661 | 11 132 439 | 1 373 776 | 1 164 159 | 3 573 397 |
| F1 | Déblocage A | 53 102 436 | 473 223 | 8 899 549 | 33 129 158 | 1 346 515 | 1 010 162 | 3 192 569 |
| F1 | Redistribution B | 66 880 452 | 220 276 | 3 749 569 | 11 423 220 | 1 367 988 | 1 168 293 | 3 581 938 |
| F2 | Témoin | 67 247 302 | 489 693 | 3 349 250 | 16 089 235 | 1 359 309 | 1 159 293 | 3 597 971 |
| F2 | Déblocage A | 52 944 938 | 891 154 | 8 370 972 | 36 638 107 | 1 331 383 | 1 034 394 | 3 287 424 |
| F2 | Redistribution B | 67 068 996 | 493 511 | 3 443 489 | 16 311 945 | 1 364 449 | 1 160 543 | 3 583 752 |

²L'énergie est donnée séparément et n'entre pas dans le bilan de matière. Minéral, détritus et structures sont également détaillés dans les JSON de résultats. Les compteurs de dons/attaques/parasitisme ci-dessus sont exhaustifs. Les lignes d'interaction des journaux sont échantillonnées à 1/256 : elles restent identifiées comme telles. Ni société, ni raisonnement, ni réciprocité adaptative ne sont déduits de ces dons.

La population ne suffit pas à décrire le renouvellement : les six populations finales sont égales mais les flux diffèrent fortement. Les recensements ont été conservés sur la grille prévue tous les 100 ticks. Un raffinement secondaire, défini avant analyse des résultats, reconstruit en plus exactement la population en fin de chaque tick à partir de toutes les naissances/morts :

| Fenêtre | Bras | Minimum en fin de tick | Ticks sous plafond | Moyenne fin de tick |
| --- | --- | --- | --- | --- |
| F1 | Témoin | 65 534 | 47/8192 | 65535,9940 |
| F1 | Déblocage A | 65 535 | 21/8192 | 65535,9974 |
| F1 | Redistribution B | 65 532 | 50/8192 | 65535,9935 |
| F2 | Témoin | 65 534 | 50/8192 | 65535,9938 |
| F2 | Déblocage A | 65 535 | 13/8192 | 65535,9984 |
| F2 | Redistribution B | 65 534 | 60/8192 | 65535,9926 |

Ce recensement ne mesure pas le nombre de places libres à l'intérieur d'un tick. Dans B/F1, les échantillons tous les 100 ticks ne voient aucun déficit alors que 50 fins de tick sont sous plafond : les deux résolutions sont distinguées, sans trace intra-tick illimitée.

## Ce qui est confirmé, réfuté, non tranché

- **Confirmé dans les deux fenêtres :** A améliore la survie courte et change fortement la composition de la population, avec moins de renouvellement réalisé et davantage de concentration des descendants à 8192. B réduit réellement la concentration initiale à quantité de cache accessible conservée.
- **Réfuté sur ces états et horizons :** la prédiction « convertir ce quart des caches en nourriture augmente le renouvellement et élargit la représentation des ancêtres à deux saisons ». Les données montrent la direction opposée. Cela ne réfute pas l'utilité des ressources pour survivre, bien démontrée à 600 ticks.
- **Non tranché :** un avantage général de B pour la descendance/diversité ; un rôle dominant de toute la concentration de matière ; les effets d'un accès donné aux occupants sans cache, d'une égalisation permanente, d'autres doses, d'autres phases climatiques ou d'horizons plus longs ; les interactions entre A et B ; le mécanisme précis de limitation de la reproduction.

Il n'y a ni p-value ni intervalle de confiance fondé sur des individus supposés indépendants. Les chiffres sont des recensements exacts de trajectoires finies. Les deux départs sont séparés de 4200 ticks, environ une saison plus 104 ticks ; leurs horizons se recouvrent et ils proviennent du même monde. **Ce sont deux fenêtres de robustesse temporelle dépendantes, pas deux réplications indépendantes.** Le rejeu déterministe de vérification n'est pas une réplication supplémentaire. Les RNG divergent normalement entre bras après intervention à cause des branches d'exécution ; seul le RNG initial est exigé identique.

## Provenance et témoin déterministe

Les checkpoints ont été copiés par un seul descripteur de fichier ouvert. Le moteur sauvegarde sur un nouvel inode puis renomme atomiquement : une copie ne mélange donc pas deux générations. Aucun arrêt, pause ni verrou du vivant n'a été nécessaire.

**F1** — tick 737400, RNG `dc1a663677e1a60c`, capture `2026-10-26T16:32:25Z`.

SHA256 état : `91b3e0877773a6eb594e31024f1fbde383637c0fbd7969261255ff7437cd6b84`.

Commit au relevé : `cfc18c312548e1f2a2630d5bb7bdc48d89d9c8a4`.

**F2** — tick 741600, RNG `fa6d234e0aeed42e`, capture `2026-10-26T16:39:03.527367+00:00`.

SHA256 état : `80b71e35f9c67744ac915ac9097af1c1cc0c7853fde07e26e182468957d3e1ba`.

Commit au relevé : `ebfcedf25682cd1579ee06eaecf2a24bc96e440a`.

ELF005 original, utilisé pour tous les ticks expérimentaux :

`c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`

Source d'origine, restée inchangée :

`10ac2a592745b2a83bd657f01961a01a3ff8ee7b8709ce0e4310f74dd3ac080c`

Le témoin F1 a été rejoué de 737400 à 738000 avec l'ELF005 exact. Son fichier entier est identique au checkpoint vivant effectivement capturé à 738000 :SHA256 `a0bc658db6efa2ec191d939b5e981b26612eb5321bc8a5929fe82b701ada5974`. La preuve antérieure 693000–693600 et ses 11069 naissances/morts,118870 dons et 59159 bénéficiaires agissant ensuite sont conservées dans `../relief-observatoire/observation-20261026-intratick.txt` ; elles n'ont pas été confondues avec les résultats nouveaux.

Le protocole a été figé au commit `4df4a5d`, poussé avant les simulations comparatives. Les outils initiaux sont au commit `ebfcedf`, les cohortes/tests au commit `7e01010` ; les compléments analytiques et résultats accompagnent ce rapport. Les seules simulations sont celles de l'ELF005 ; l'instrumentation hors moteur ne consomme pas son RNG.

## Contrôles, charge et stockage

Les tests `check.py` ont réussi : transformations répétées identiques, vérification indépendante des champs binaires permis, conservation et règle d'arrondi/redistribution, checksum corrompu rejeté, refus de destination existante et du vivant même via symlink. Tous les octets des agents, de la configuration et du RNG initial sont identiques ; seuls cache/nourriture autorisés et checksum diffèrent. Sauvegarde/rechargement par le chargeur d'origine contrôlés.

Le selftest analytique vérifie les descendants après mort de la racine, plusieurs générations, la séparation des liens familiaux déjà présents au départ, la cohérence des flux, les dons échantillonnés et l'extinction absorbante sans fondateurs. Une relecture indépendante a recoupé les formules, des différences binaires et la généalogie effective du témoin. Les flux exhaustifs de chaque lignée et chaque état vivant reconstruit concordent avec les exports.

Au total : **49 752 ticks simulés**, dont 49152 pour les six bras et 600 pour la vérification technique, sous la limite 60000. Simulations séquentielles plafonnées à 0,75 CPU et 256 Mio, sans réseau, avec seulement le dossier de copie en écriture. Durée au mur du pilote :523,5 secondes ; somme des appels comparatifs :514,2 secondes. Pas de journal intra-tick ajouté.

Budget de fichiers :2 Gio. Le manifeste post-analyse donne le volume effectif et toutes les empreintes ; le volume au terme du pilote était 1 740 077 776 octets (1,621 Gio). Les sorties de cohorte et bilans ultérieurs sont incluses dans l'inventaire final :1 756 181 930 octets (1,636 Gio),221 fichiers bruts. Aucun conteneur d'expérience permanent ne subsiste.

`histoire/STOCKAGE.txt` et la stratégie de stockage ont été lus avant les essais. Au relevé préflight :318 jours projetés,statutok. Dernier relevé de stockage disponible au postflight :312 jours projetés,0,838 Go/jour,histoire 1,861 Go,statutok ;243 Gio libres mesurés aussi pardf. **`histoire/stockage.log` était absent** : les segments n'avaient pas encore 24 heures et aucune recompression éligible n'était attendue ; cela ne vaut pas un journal vide certifié sans erreur. Aucune action de stockage niveau 3, aucune suppression d'histoire.

Le vivant a progressé du tick 740400 au tick 745200 pendant le pilote ; au contrôle supplémentaire 16:45:59Z, tick 745800,population 65536,matière 76840404. Même conteneur etPID647691,StartedAt 25/10/2026 19:56:08Z,RestartCount 0,aucunOOM,ELF et source exacts. Aucun ralentissement anormal observé aux relevés, sans prétendre à un benchmark de latence complet. Aucun ancien monde ni E1 n'a été relancé, aucun fondateur recréé, aucune loi déployée ni fusionnée dans le vivant.

## Prochaine étape recommandée

Conserver Relief vivant tel quel. Sur copies, compter de façon bornée les causes de mort (âge/famine/attaque) et les refus de reproduction (plafond, voisin occupé, énergie, corps, réserve). Vérifier d'abord que cette observation passive reproduit l'ELF005 octet pour octet. Puis tester une dose plus petite de conversion, par exemple 1/16, aux mêmes horizons. Cela permettrait de distinguer l'amélioration de survie, la raréfaction des places et un éventuel changement de capacité reproductive. L'étude actuelle ne justifie aucun changement de loi dans le vivant.

## Lire et reproduire les preuves

Racine locale : `/workspace/produits/octet-adn/experiments/relief-comparatif-20261026/`.

- `PROTOCOLE.md`, `preregistration.json` : décisions avant les runs et empreinte du protocole.
- `results/summary.json`, `results/windowN-analysis.json` : chiffres et définitions ; `windowN-comparison.tsv` : contrastes ; `windowN-lineage-results.tsv` : toutes les lignées, dénominateurs et ressources normalisées.
- `results/run-manifest.json` : commandes Docker exactes, durée, états et empreintes par segment ; `replay-verification.json`, `check-results.json`, `postflight.json` : contrôles.
- `results/evidence-manifest.json` : inventaire et SHA256 de tous les fichiers bruts retenus, états, exports, événements et cohortes.
- `data/windowN/base/state.ck` : captures initiales ; dossiers `control/`, `unlock/`, `redistribute/` : checkpoints initiaux et 600/4096/8192, `transformation.json` et `transformation.cells.tsv`, événements et séries.
- `README.md` : commandes de compilation, tests, analyse et reproduction. `reproduce.py` repart des deux captures figées, sur un nouvel output, sans lire le vivant ni invoquerinit.

Les gros artefacts restent locaux dansdata, ignorés pargit ; les sources, contrôles, manifestes, agrégats et rapport sont versionnés et poussés. La reproduction exige ces captures conservées ; les manifestes seuls ne permettraient pas de reconstituer un état disparu.
