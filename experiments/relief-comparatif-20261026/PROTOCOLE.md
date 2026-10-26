# Relief : protocole comparatif figé le 26/10/2026

Mission b8e25e22-c4a1-4e91-814f-a8dd2ce11876. Préenregistrement avant simulation comparative. Aucun changement des lois, du vivant, de son ELF, de son superviseur ou des anciennes écologies. Copies transitoires, population zéro = extinction sans fondateurs. Aucune inférence de société ou de raisonnement depuis les dons.

## État et preuve technique

Source : checkpoint existant `experiments/vie-relief-20261025/state.ck`, copié par un seul descripteur ouvert (le moteur écrit un autre inode et renomme atomiquement). Aucun verrou du vivant pris. Première copie : tick737400, population65536, RNG `dc1a663677e1a60c`, matière76840404, SHA256 `91b3e0877773a6eb594e31024f1fbde383637c0fbd7969261255ff7437cd6b84`, commit `cfc18c312548e1f2a2630d5bb7bdc48d89d9c8a4`.

Toutes les simulations utilisent **l'ELF005 original**, SHA256 intégral `c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`. Le témoin est également rejoué jusqu'à un checkpoint vivant ultérieur effectivement capturé ; égalité exigée sur le fichier entier, RNG et compteurs compris. L'ancien témoin693000–693600 reste dans `../relief-observatoire/observation-20261026-intratick.txt` ; il n'est pas une réplication.

## Ce que signifient les interventions

Lecture préalable : `relief.c`, README/EXPLOITATION/stockage/STRATEGIE et preuve intra-tick. **Aucun droit de détention n'existe dans ELF005** : toute cache est commune à tout occupant présent. `Cell.owner` est seulement une trace du constructeur et n'est jamais consulté pour retirer du cache. Ici, « détenteur » désigne donc un occupant physique initial d'une case à cache positive, jamais un propriétaire juridique ou social.

Trois bras depuis le même fichier, sans appel au RNG pendant la transformation :

- **Témoin** : copie octet pour octet, aucun champ changé.
- **Déblocage A** : pour toute case i, m_i=floor(cache_i/4), cache_i-=m_i et food_i+=m_i. Tous les occupants, positions, traces owner et autres champs restent identiques. Chaque somme locale food+cache est conservée, les caches positives restent positives. L'arrondi est documenté ; la dose exacte est calculée. C'est une conversion matérielle locale en nourriture consommable, productrice d'énergie et perceptible : cela modifie aussi sa décomposition (1/64 contre1/256), les perceptions et éventuellement la production solaire au seuil food96. Ce n'est pas un effet pur de quantité de matière ni une libération directement dans les inventaires.
- **Redistribution B** : seules les cases initialement occupées avec cache>0 participent. Regroupement par i modulo256 (phase de décomposition). Pour chaque groupe de n cases et total C, chacune reçoit floor(C/n), puis les C modulo n premières cases par index croissant reçoivent +1. Nourriture, inventaires, positions et cellules inoccupées restent identiques. Total global et total immédiatement sous les occupants inchangés, support des caches positives inchangé, total par phase inchangé. L'inégalité des quantités entre ces occupants est réduite ; les écarts entre groupes/territoires peuvent subsister. Cela ne crée pas de nouveaux ayants accès. La décomposition agrégée initiale reste identique ; sa trajectoire ultérieure peut changer par épuisement/retrait/dépôt, qui sont des conséquences.

Les identités/états des agents, tout le World sauvegardé, configuration/RNG/compteurs doivent être strictement identiques après transformation. Seuls mots cache/food autorisés et checksum peuvent différer. Conservation exacte, support et totaux par phase contrôlés. Recharge par le lecteur d'origine après sauvegarde. Les audits de manipulation vérifient l'effet réel sur l'inégalité avant les simulations.

Les estimands sont **A moins témoin** et **B moins témoin** ; A et B ne sont pas des doses comparables. Aucun bras combiné prévu : aucune interaction A×B ne sera revendiquée. Un éventuel besoin de combinaison sera une prochaine étude et non un choix après sélection de résultats.

## Horizons, critères et robustesse fixés

Horizon primaire8192ticks (deux saisons4096), intermédiaires600 et4096. L'ELF est repris aux horizons sans altérer le RNG. Mesures :

- naissances/décès exhaustifs, naissances/N0, population minimale aux échantillons100ticks, déficit de capacité aux échantillons ; les vacances intra-tick ne sont pas mesurées ;
- pour chaque lignée fondatrice : effectif/part initiale et finale, naissances/décès, survie des individus présents initialement, descendants vivants de ces individus, reproduction directe ;
- pour chaque individu initial : vivant à H, enfants directs, descendants vivants hors lui-même, profondeur de descendance ; chaque nouveau-né est rattaché via son parent dans son propre bras, jamais par identité numérique entre bras ;
- diversité des génomes exacts, diversité par empreintes signalée séparément, entropie et nombre effectif des lignées ;
- matière par réservoir, énergie, cache accessible sous occupants, cache par occupant et par lignée, concentration brute et normalisée par effectif ;
- compteurs exhaustifs dons/attaques/parasitisme, échantillons d'événements explicitement étiquetés1/256. Aucune trace intra-tick supplémentaire.

Hypothèses testées descriptivement : A augmente le renouvellement ou les descendants vivants ; B modifie l'avantage relatif des lignées à quantité de cache accessible constante. Absence d'effet ne prouve pas absence générale de limitation. Pas de p-value ni intervalle de confiance fondé sur des individus interdépendants.

**Seconde fenêtre** : au premier moment après disponibilité des outils, protocole figé et checkpoint vivant d'au moins737400+4096, capturer le dernier checkpoint naturellement disponible ; répéter les trois bras et les mêmes horizons. Attendre si nécessaire sans agir sur le vivant. Cette fenêtre est dépendante du même monde/histoire, pas une réplication indépendante. Les directions, amplitudes et éventuelles inversions seront toutes rapportées. Répétition déterministe = vérification technique seulement. Aucun réensemencement/RNG alternatif.

## Budget et conservation des preuves

Préflight lu avant travaux : STOCKAGE au16:27Z, libre262,4Go, histoire1,836Go, rythme0,826Go/jour,318jours projetés, statut ok. `stockage.log` absent avant première recompression (>24h), à signaler comme absent et non comme contrôle vierge. Aucun niveau3. Disque partagé mesuré aussi par df.

Budget expérimental : **2Gio de fichiers**, **0,75CPU au maximum et faible poids CPU**, une simulation à la fois,256Mio RAM, réseau coupé, rootfs lecture seule, montage unique du répertoire d'essai en écriture. Maximum total simulation60000ticks, délai3000s par appel et2h de calcul cumulé au mur ; arrêt des essais seuls si limite atteinte, aucun signal au vivant. Comparaisons principales49152ticks, vérification technique<=6000ticks. Surveiller progression du vivant entre les bras.

Les sorties brutes restent dans `data/` ignoré par git avec empreintes/manifeste ; les sources, protocole, rapports et résultats agrégés sont versionnés. Aucun fichier étranger sale n'est embarqué. Commandes et provenance des binaires gardées. Les essais sont bornés et ne deviennent pas de nouveaux mondes permanents.
