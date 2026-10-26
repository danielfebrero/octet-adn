# Transmission — 26 octobre 2026

**Monde distinct démarré sur ordre de Daniel, premier run effectué, puis laissé vivant.** Relief et son ELF005 sont intacts ; les 34 anciens mondes ne sont jamais relancés ; E1 reste éteint. Aucun organisme ni état de Relief n'est importé.

Le moteur est un ELF natif C11 statique. Aucun modèle de langage, interpréteur, appel système à la disposition des organismes ni texte généré sous leur nom. Le broker réseau et le collecteur sont des outils Python de l'expérimentateur, séparés de la VM.

## Information confiée

Le [document français initial](initial-fr.txt), **1 174 octets**, explique les organismes, le monde simulé, le VPS, Daniel, les humains, Grok et Codex, la transmission et le courrier. SHA256 : `2260e4d6466b9a44c21a9bb18b668a6e6e63e8ec8fa5fe3b32fc3adb65ae5a7b`.

Les **fondateurs 1 à 8, lignées 1 à 8**, reçoivent chacun une copie en RAM. Les autres n'en reçoivent aucune. Aucune instruction ne peut ensuite lire la source. La cible d'évaluation demeure dans une zone native du checkpoint que l'ISA ne peut adresser ; elle compare uniquement une restitution explicite. Ni bibliothèque, ni broker, ni artefacts ne contiennent une copie gratuite du document.

Ces huit conservateurs ont un **programme écrit par nous**, qui lit sa RAM, restitue ses octets et tente de les envoyer. La copie de RAM à la reproduction est une règle du moteur. Les 24 autres fondateurs sont des échantillonneurs écrits par nous : lecture de bibliothèque ou octets pseudoaléatoires, stockage local/partagé et courrier. Les mutations peuvent modifier ces programmes. Aucun résultat n'est présenté comme émergence depuis des octets aléatoires ni comme compréhension.

## Capacités et récompense

128 places, 32 fondateurs, code modifiable de 512 octets, RAM de 4 096 octets, deux registres de 8 bits, pointeur de 16 bits, sortie binaire de 4 096 octets et huit artefacts partagés de 4 096 octets. Conservation possible en RAM, code, artefact ou combinaison. Un autre encodage doit être décodé par **les instructions du programme** avant restitution. Aucune routine native ne cherche ou ne décode une représentation pour eux.

32 instructions maximum/tick. Une instruction et l'entretien coûtent chacun une unité d'énergie. EAT termine le tick et apporte six unités : alimentation abstraite, sans conservation physique de matière. Reproduction à partir de 3 000 unités, coût 80 puis partage d'énergie et copie code/RAM. Une erreur RAM et une erreur code sont chacune possibles avec probabilité 1/4. Âge maximal 16 384 ticks ; nouveau-nés inactifs le tick de naissance. Aucun réensemencement après extinction.

La restitution compare **longueur et tous les octets**. Seule une égalité parfaite rapporte de l'énergie : `1024 + 2 × longueur`, bornée par le plafond de 4 096, coût 16, au plus un bonus/individu/512 ticks. Aucun retour ne révèle l'octet attendu. `perfect` compte les égalités, `awarded` les bonus effectivement attribués. Conserver ne prouve pas comprendre.

| Opération | Effet |
| --- | --- |
| 0 NOP, 1 IMM | Attendre ; charger un littéral dans A |
| 2 LOAD, 3 STORE | Lire le support sélectionné ; écrire RAM[P] |
| 4 NEXT, 5 PREV, 6 ZERO | Déplacer ou remettre à zéro P |
| 7 JUMP, 8 JZERO | Saut absolu 16 bits, direct ou si A est nul |
| 9 EMIT | Ajouter A au flux de sortie |
| 10 EAT, 11 SPLIT | Alimentation ; reproduction |
| 12 RESTORE, 13 MAIL | Soumettre la sortie au vérificateur ; tenter de la poster |
| 14 RANDOM, 15 XOR, 16 ADD, 17 SWAP | Octet aléatoire ; calcul A/B |
| 18 SOURCE, 19 BOOK | Choisir RAM/code/partagé/bibliothèque et case 0..7 |
| 20 REQUEST, 21 OUTCLEAR | Demander un titre depuis la sortie ; vider la sortie |
| 22 SETLO, 23 SETHI, 24 GETLO, 25 GETHI | Lire/écrire les octets de P |
| 26 INC, 27 DEC | A +1 / -1 |
| 28 SHAREDSTORE, 29 CODESTORE | Écrire artefact ou code, coût supplémentaire 2 |
| 30 SENSE, 31 YIELD | Lire son énergie ; terminer le tick |

## Grokipedia exclusivement

Le moteur a **`--network=none`**. Une demande est un titre ASCII de 1..96 caractères, sans slash, URL, pourcentage ni query. Le broker reconstruit uniquement `https://grokipedia.com/page/TITRE`, vérifie TLS, refuse les redirections et désactive les proxies implicites. Il ne suit aucun lien ni ne charge image, CSS ou script. Il ne monte que `bridge/`, sans checkpoint, cible ni courrier.

Demande : 64 unités d'énergie, au plus une/individu/4 096 ticks. Les lectures d'octets coûtent des instructions et du temps. Huit cases de requêtes, avec remplacement possible d'une demande précédente. Broker : 64 tentatives par trois heures réelles, 2 Mio HTML/réponse, 64 Kio de texte/article, cache de 8 Mio sans éviction. Balises et scripts retirés. Import moteur aux multiples de 64 ticks. Le réseau est une entrée extérieure ; déterminisme seulement à entrées identiques.

Quatre demandes réellement servies : [Artificial life](https://grokipedia.com/page/Artificial_life), [Information theory](https://grokipedia.com/page/Information_theory), [Error correction code](https://grokipedia.com/page/Error_correction_code), [Evolution](https://grokipedia.com/page/Evolution). **Les quatre textes sont tronqués à 65 536 octets.** [live-library.txt](live-library.txt) et le cache enregistrent URL, date, SHA256 HTML/texte, longueurs et troncation. Le HTML brut n'est pas conservé ; son empreinte ne permet pas de le reconstruire.

## Courrier et relève

Les lettres sont les octets produits par la VM. Texte, petite image ou format inventé sont acceptés ; aucun format n'est exécuté et aucun bonus esthétique n'existe. Limites : 4 Kio/lettre, une admission globale/512 ticks et une/individu/2 048 ticks après le premier envoi ; 16 384 lettres ou 64 Mio de charge utile au total. Une limite atteinte refuse les envois mais **laisse vivre le monde**. L'ordre des agents peut favoriser les premières places pour le courrier.

Chaque `.bin` possède son `.tsv` : tick, ID, parent, lignée, naissance, fondateur/descendant, programme d'origine, taille, checksum. Fichiers et répertoire synchronisés avant le checkpoint. `mail.committed` est publié seulement après synchronisation du checkpoint. À la reprise, les fichiers au-delà du checkpoint sont retirés avant rejeu ; ils ne doivent pas être relevés comme validés.

Le [collecteur](collect.py) vérifie taille et checksum du checkpoint atomique, limite la relève au checkpoint ET au curseur, vérifie chaque original et ajoute son SHA256 au manifeste. `--mark` sauvegarde ce manifeste et avance le curseur de relève sans supprimer les originaux. Une simple lecture n'utilise pas `--mark`. L'interprétation humaine reste séparée. L'agenda EG du topic assure la relève toutes les trois heures : première relève régulière **27 octobre à 01:32 UTC**, après le relevé initial de 22:32 UTC.

```sh
cd /workspace/produits/octet-adn/experiments/transmission-20261026
python3 collect.py ../vie-transmission-20261026             # lecture sans relève
python3 collect.py ../vie-transmission-20261026 --mark      # relève effective
```

Voir [RESULTATS.md](RESULTATS.md), les [six originaux et leur provenance](first-mail.zip), le [manifeste](first-mail.json), les [contrôles](reward-control.txt) et la [validation ASan/UBSan](validation-san.txt). Les 24 fondateurs explorateurs sont morts dans le premier run. Les survivants sont issus des conservateurs. Récompense et programmes initiaux orientent fortement l'expérience. Aucune compréhension, création sémantique ou intelligence générale n'est démontrée. De futures mutations ne sont pas une promesse de nouveauté.

## Vie durable et reprise

État : `/workspace/produits/octet-adn/experiments/vie-transmission-20261026`. ELF immuable : `build-prod-001/transmission`, SHA256 **`800aac0745a36c5ec78a35d360629a03977843ae2c707792437cbf83c5359c74`**. Source figée à côté et [SHA256SUMS](SHA256SUMS). Ne jamais substituer un autre ELF ni reconstruire à ce chemin.

`octet-transmission` exécute [vivre.sh](vivre.sh), PID1, fenêtres de 600 ticks à 100 ms. Checkpoint tous les 256 ticks et à l'arrêt. Relais TERM/INT, attente de sauvegarde, reprise du checkpoint uniquement, sortie normale si population nulle. Docker : réseau coupé, moteur/racine en lecture seule, utilisateur 10003:999, 0,5 CPU, 64 Mio, 16 PID, capacités Linux retirées, logs 2 × 2 Mio, reprise `on-failure:3`. Le broker `octet-transmission-library` a 0,25 CPU, 96 Mio, 16 PID et les mêmes restrictions de droits et logs ; seul son code strict accède au réseau.

Checkpoint natif little-endian v1 : mémoires, livres importés, RNG, compteurs, horloge. FNV64 détecte la corruption accidentelle, sans signature cryptographique. Écriture temporaire, fsync, renommage atomique, fsync du répertoire. `flock` refuse deux écrivains. SIGKILL peut perdre les ticks non sauvegardés ; leurs lettres ne sont pas validées. Une requête réseau non servie peut également être perdue dans un crash ; pas de promesse de rejeu déterministe du réseau.

État, courrier, cache, requêtes, état du broker et logs sont bornés. Les originaux ne sont pas supprimés. Un courrier/cache plein coupe seulement son canal. Le répertoire reste inférieur à environ 256 Mio avec ces limites et une relève régulière sans copies superflues ; surveiller l'espace hôte.

```sh
build-prod-001/transmission inspect ../vie-transmission-20261026
docker logs --tail=15 octet-transmission
# Seulement après preuve de l'absence de l'écrivain, même ELF et même état :
build-prod-001/transmission run ../vie-transmission-20261026 0 100
```

`init` exige un répertoire absent et n'a été exécuté **qu'une fois** pour ce monde. Le superviseur ne contient aucun init. L'extinction demeure un résultat ; aucune fondation nouvelle n'est autorisée par une reprise.

## Vérification isolée

```sh
make BUILD=build-test-NEUF test
make BUILD=build-san-NEUF CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
cc -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -o /tmp/transmission-control reward-control.c
/tmp/transmission-control initial-fr.txt
```

Ces tests utilisent uniquement de petites populations en mémoire ou des répertoires temporaires, jamais le vivant : restitution exacte, corruption non récompensée, vrai bytecode XOR depuis artefact partagé, cooldown, contrôle sans bonus, URL refusée, reprise bit à bit, courrier, verrou, SIGTERM et extinction sans réensemencement.
