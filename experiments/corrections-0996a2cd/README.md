# Corrections mesurées — ordre 0996a2cd

Base lue : `origin/main` à `41ab85e`. Les corrections restent dans
`experiments/`, sans modifier `src/`, l'ISA, les seeds ni les labos historiques.
Ce dossier ne propose pas une quatrième approche : il éprouve les imperfections
signalées dans la mission. Les mesures ci-dessous n'ont pas servi à ajouter
un second mécanisme au rendez-vous.

## Rendez-vous : de la présence à l'exécution

**Faux auparavant.** `capable()` cherchait un capteur et STEP dans tout le
génome décodé. Un YIELD avant ces instructions, un saut qui les évite ou un
ip après elles ne changeait rien au verdict.

**Diff.** `rendezvous_capable()` exécute un seul `vm_run` depuis l'état courant,
dans une copie profonde de `World`, des créatures présentes et des nouveau-nés,
et du RNG. Le capteur doit être exécuté avant STEP dans ce tick. La VM et les
actions du monde sont compilées depuis les sources existantes sous des noms
privés : aucun interprète réécrit, aucun watcher ni TLS de mutation partagé
avec le vrai tick. Les enfants éventuels de la sonde sont libérés. Le test
n'est exécuté qu'à la période du rendez-vous et si une marque est à portée.
Le capteur accepte SENSE, LOAD, SENSE_NB, SENSE_EN, SENSE_STIM, SENSE_HIST et
NEURAL. Le flag du profil reste celui qui existe ; flag éteint, aucun appel
à la sonde et aucune allocation.

**Mesure C.** Les 16 seeds du dépôt sont évaluées avec énergie 220, ip 0,
registres nuls et nourriture 14, sans évolution du monde. Une dérive distincte
part des mêmes 16 lignées, graine `0x0996A2CD60`, `mutate()` réel 0,02/0,05,
60 générations sans sélection. Une ligne de dérive est un instantané de
génome, pas une répétition indépendante. Aucun génome n'est modifié par la sonde.

| Génération | Porte statique | Tick exécuté |
|---:|---:|---:|
| 0, seeds | 14/16 | 14/16 |
| 10 | 14/16 | 10/16 |
| 20 | 14/16 | 9/16 |
| 30 | 14/16 | 8/16 |
| 40 | 14/16 | 7/16 |
| 50 | 14/16 | 5/16 |
| 60 | 14/16 | 5/16 |
| Total des 61 instantanés | 853/976 | 497/976 |
| Plage par instantané | 13..14/16 | 4..14/16 |

Les seeds repair et general sont refusées par les deux portes ; les 14 autres
passent. **Oui : les instructions doivent maintenant être exécutées.** Le
gain de sélectivité est observé sur la dérive ; la porte ne sépare pas les seeds
de départ. Ce n'est pas une preuve que le capteur cause le mouvement, ni une
preuve de meilleure évolution ou d'apparition de sociétés.

Vérifications : neutralité binaire de `World`, de ses 16 créatures et du RNG
après sonde ; aucune invocation du watcher du vrai monde ; ce watcher est
encore actif au vrai tick suivant. Une sonde SPLIT avec le vrai multiplicateur
de mutation posé à 3 ne crée aucune naissance dans le TLS réel ; le tick réel
qui suit conserve le taux 0,06. STORE, EAT, SPLIT, ATTACK et EMIT sont aussi
exécutés sur copies dans le selftest du module. Les queues mortes, l'ordre
STEP puis SENSE et l'ip sur YIELD sont refusés.

Coût indicatif, compilation `-O2`, 4096 sondes par ligne, état remis identique
à chaque sonde (mesure a04 sur cette machine ; temps non portable) :

| Population copiée | Secondes | µs/sonde |
|---:|---:|---:|
| 1 | 0,010076 | 2,460 |
| 128 | 0,032776 | 8,002 |
| 4096 | 0,850709 | 207,693 |

La copie coûte O(population) ; à 4096 individus, cet essai ne prétend pas
qu'un monde dense conserve la vitesse d'avant. Aucun monde long n'est lancé.

## Nurserie : plafond 4096 et place locale

**Imparfait auparavant.** Les balayages à plafond 160 sur 512 cases n'avaient
jamais exercé les refus de place ou de plafond. Les selftests ne couvraient
que des petits plafonds. `CMAX` vaut déjà **4096**, pas 1024.

**Diff.** Aucun paramètre ni mécanisme de naissance n'est changé. Un nouveau
cas dans `naissance_selftest()` et le banc ci-joint testent le vrai plafond.
Pour chacune des 32 graines 1..32 : 4096 adultes sur 32×32 cases (quatre par
case), un œuf payé, puis trois appels sans pas VM :

1. À 4096 adultes, l'œuf mûr attend sous le vrai cap 4096.
2. À 4095, après retrait d'un occupant loin du nid, toutes les cases restent
   occupées : une place globale disponible ne suffit pas.
3. Tous les occupants d'une case voisine sont déplacés ailleurs ; l'œuf
   éclot dans cette case vide, sans second paiement.

| État, 32 graines | Refus plafond | Refus place | Éclosions |
|---|---:|---:|---:|
| 4096 adultes | 32 | 0 | 0 |
| 4095, pas de case vide près du nid | 0 nouveau | 32 | 0 |
| 4095, une case voisine libérée | 0 nouveau | 0 nouveau | 32 |

Coût total : **640 énergie et 96 nourriture**, soit exactement 20 et 3 par
œuf ; population finale 4096 dans 32/32 cas. **Oui : les deux leviers de refus
sont exercés et l'attente fonctionne sans repaiement.** La lacune de validation
est corrigée ; aucun défaut d'éclosion n'a été trouvé. Ce banc contrôlé
n'affirme pas que ces densités apparaissent spontanément dans un monde vivant.

## Mondes protégés : une liste commune de 14 noms

**Faux auparavant.** Les listes séparées de `rencontres.c` et `garde.c`
oubliaient `vie-rencontres-b129054c` et `vie-rencontres-sonnet-41ab85e`.

**Diff.** `mondes_proteges.h` devient leur liste commune de 14 noms et leur
reconnaissance de dernier composant. `rencontres.c` garde la résolution des
liens existants avant ce test. La garde exige désormais 14 enregistrements ;
une ancienne preuve de 12 mondes est refusée plutôt que prise pour une
vérification de tous les vivants. Le format reste `OCTET_GARDE_V1`, avec son
nombre explicite. Le vieux test de garde est ajusté à la même liste mais
**n'est pas exécuté**, car il invoque la commande snapshot.

| Vérification | Résultat |
|---|---:|
| Dernier composant exact, chemin et `/` terminaux | 14/14 reconnus |
| Noms suivis de `-essai` | 14/14 laissés disponibles |
| CLI run sur chemins neufs portant les 14 noms | 14/14 refusés avant mkdir |
| Faux `/proc` privé, 14 processus | 14/14 lus |
| Preuve de fixture écrite directement par le banc C, puis relue | 14/14 identiques |
| Absence ciblée de chacun des deux nouveaux mondes | 2/2 refusées |

Le banc appelle les fonctions internes de lecture sur son faux `/proc`,
jamais `garde snapshot`, jamais `/proc` de l'hôte. Aucune conclusion sur la
santé des mondes de l'hôte n'est tirée de ces fixtures.

## Sonde de phase : grief obsolète, reproduit sans diff de détecteur

Sur `origin/main` lu pour cette mission, `sonde_phase.c` et son README
contiennent déjà la v2 : lésion de la cible du prochain STORE, une seule case,
restitution de sa période et réécriture par les deux jumeaux. La zone fixe
16..31 de la v1 n'est plus utilisée. Le défaut de PR #18 décrit dans l'ordre
ne s'applique donc plus à cette base.

| Contrôle relancé, détecteur inchangé | Compte |
|---|---:|
| Moteur pas à pas égal à exec64 | 64/64 bandes |
| Inerte | 0/32 fausse réparation |
| Aléatoire | 0/32 |
| Cristal | 0/32 |
| Positif, quatre phases fixes | 4/4, seuil ≥3 |
| Sans lésion | 0/32 |
| Barre de phases déjà fixée | 31/32, seuil ≥28 |

**Non : aucune nouvelle correction de sonde n'est nécessaire sur cette base.**
Ces commandes rejouent les contrôles existants ; elles ne relancent ni fuzz,
ni soupe, ni mix. Les résultats bruts vont dans `/tmp`.

## Reproduction et validation

Commandes depuis la racine, aucun `make` global. Chaque compilation ci-dessous
crée un chemin ELF jusque-là absent. Les sources du cœur sont liées, mais
aucun essai soupe n'est lancé.

```sh
correction_build=$(mktemp -d /tmp/octet-corrections-build-XXXXXX)
correction_core='src/rng.c src/isa.c src/vm.c src/mutate.c src/world.c src/labs.c src/store.c src/runner.c src/seeds.S src/labs_h.c src/novelty.c src/soup.c'
cc -O2 -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-format-truncation -Iinclude -pthread -o "$correction_build/corrections" experiments/corrections-0996a2cd/corrections.c experiments/rencontres-ligne/naissance.c experiments/rencontres-ligne/rendezvous.c $correction_core -lm -ldl
"$correction_build/corrections" selftest
"$correction_build/corrections" run
cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Iinclude -pthread -o "$correction_build/corrections-san" experiments/corrections-0996a2cd/corrections.c experiments/rencontres-ligne/naissance.c experiments/rencontres-ligne/rendezvous.c $correction_core -lm -ldl
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$correction_build/corrections-san" selftest
cc -O2 -std=gnu11 -Iinclude -o "$correction_build/historiques" experiments/corrections-0996a2cd/historiques.c $correction_core -lm
"$correction_build/historiques"
cc -O2 -std=gnu11 -Iinclude -o "$correction_build/sonde-phase" experiments/sonde_phase.c $correction_core -lm
"$correction_build/sonde-phase" selftest
"$correction_build/sonde-phase" run "$correction_build/sonde-controles.txt"
"$correction_build/sonde-phase" barre "$correction_build/sonde-barre.txt"
```

Exécuté : banc a04 `run`, banc a05 `selftest`, banc a05 ASan+UBSan avec détection
des fuites, tous verts. `terrarium` a04 `selftest` : échange=0, naissance=0,
rendezvous=0 échecs ; profil désactivé identique hash+RNG aux hooks contournés ;
`Params=416` avec assertion de compilation ; reprise identique du monde, RNG,
œufs, cooldowns, marques et des quatre fichiers TSV. Le banc `historiques`
n'appelle que `test_core_state_hash_matches_main()` : **10/10 hashes historiques
inchangés** (un cycle de 50 ticks par labo), sans lancer la suite globale.

Les ELF de mesure a01..a05 restent hors dépôt. Les sorties brutes de la mesure
a04 sont archivées dans [results.txt](results.txt), celles de la sonde existante
dans [sonde-controles.txt](sonde-controles.txt) et [sonde-barre.txt](sonde-barre.txt).
Les selftests et l'historique ci-dessus sont une transcription des sorties
outils, sans fichier journal brut conservé. Une erreur
de construction initiale a01 (stockage de copie trop volumineux en TLS)
débordait la pile d'un thread au selftest de reprise ; le stockage de copie
est passé sur le tas avant les mesures a04. Le selftest complet de reprise
passe après cette correction. Aucun monde de l'hôte n'a été inspecté,
arrêté, relancé ou relinké ; aucune loi partagée n'a été copiée.

À l'intégration, trois cibles sûres supplémentaires donnent accès aux bancs :

```sh
make experiments/approches-build/IDENTIFIANT_NEUF/corrections
make experiments/approches-build/IDENTIFIANT_NEUF/historiques
make experiments/approches-build/IDENTIFIANT_NEUF/sonde-phase
experiments/approches-build/IDENTIFIANT_NEUF/corrections selftest
experiments/approches-build/IDENTIFIANT_NEUF/historiques
experiments/approches-build/IDENTIFIANT_NEUF/sonde-phase selftest
```

Ces trois builds et leurs tests ont aussi été exécutés à l'intégration. Les
cibles des trois nouvelles approches refusent `make -B` sur un ELF déjà présent ;
les trois empreintes SHA-256 sont identiques avant et après ces tentatives.
