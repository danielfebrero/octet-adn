# Premier run réel et maintien vivant

Fondation unique le **26 octobre 2026**, seed 20261026, 32 fondateurs dont 8 conservateurs. Premier segment jusqu'au tick 256, demandes réelles Grokipedia servies, puis poursuite du **même checkpoint** jusqu'au tick 7 000. Le réseau est une entrée réelle horodatée.

| Mesure au tick 7 000 | Résultat |
| --- | ---: |
| Population / descendants vivants | 128 / 120 |
| Naissances / décès | 123 / 27 |
| Erreurs de copie code ou RAM | 53 |
| Restitutions tentées / exactes | 299 / 235 |
| Exactes par descendants | 195 |
| Bonus attribués / énergie ajoutée | 235 / 755 535 |
| Livres importés | 4 × 65 536 octets, tous tronqués |
| Lectures d'octets bibliothèque par la VM | 11 789 |
| Écritures d'octets partagés | 7 267 |
| Lettres / octets | 6 / 6 126 |

[Sortie moteur](first-run.txt). Les conservateurs survivants exécutent des programmes amorcés par nous. Les 24 fondateurs explorateurs sont morts et ne sont pas remplacés par de nouveaux fondateurs.

La première lettre : tick **533**, fondateur **16**, parent 0, lignée 16. Ses 256 octets sont **42 NUL** lus avant disponibilité du livre puis **214 octets égaux aux positions 42..255** du texte extrait de Grokipedia Evolution. L'échantillonneur est écrit par nous. Aucun message rédigé ni création sémantique démontrée. SHA256 : `b61f7ea889085b82fc3b522cde363c32bbf46a4ba8393375dcc4486c62365c21`.

Les cinq lettres suivantes viennent des fondateurs 1 et 2 et sont les **1 174 octets exacts du document confié**. Elles prouvent courrier et copie fidèle. Les 195 restitutions exactes par descendants sont un autre compteur du moteur ; elles ne sont pas ces cinq lettres fondatrices. Aucun texte ajouté sous leur nom.

## Contrôles séparés

[Contrôle du document réel](reward-control.c), huit conservateurs sans reproduction ni mutation : **8/8 vivants au tick 10 000 avec bonus**, contre **0/8 au tick 4 070 sans bonus**. Respectivement 64 et 24 restitutions exactes. La récompense améliore causalement la survie de cette politique ; ce n'est pas une preuve d'évolution spontanée vers la préservation.

Autre contrôle en mémoire : 200 000 ticks sans réseau ni écriture disque, 128 vivants, 1 664 naissances, 1 568 décès. Il exerce le renouvellement, sans garantir la stabilité future du monde réel.

Tests C, CLI, broker, ASan et UBSan passent. Le décodage alternatif est bien exécuté par **LOAD/XOR/EMIT/RESTORE dans la vraie VM**, depuis un artefact partagé. Checkpoints directs/repris identiques, courrier identique après rollback de queue non commise, corruptions rejetées, double écrivain refusé et arrêt SIGTERM repris.

## Deux regards sur la vie continue

`octet-transmission` : démarrage **22:32:30 UTC**, PID hôte **778547**, zéro redémarrage. Broker : même instant, PID **778609**. ELF monté en lecture seule. Les deux continuent après la tâche.

| Regard | Tick sauvegardé | Population | Naissances / décès |
| --- | ---: | ---: | ---: |
| Premier regard après lancement | 7 168 | 128 | 123 / 27 |
| 22:34:07 UTC | 7 936 | 127 | 123 / 28 |

Horloge et mortalité avancent. Ces checkpoints cohérents peuvent être en retard de 255 ticks sur le processus.

Avant et après : `octet-relief`, PID **647691**, démarrage **25 octobre à 19:56:08 UTC**, zéro redémarrage ; ELF005 SHA256 **`c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`** inchangé. Aucune commande run/init ni signal adressé à Relief, E1 ou aux 34 archives.
