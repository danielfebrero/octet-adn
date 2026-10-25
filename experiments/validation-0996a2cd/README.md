# Corrections vérifiées, ordre 0996a2cd

Base après `git fetch origin` : `41ab85ea270e8748267eae4f0df6b92c7a30fc0e`.
Worktree neuf, branche `codex/trois-approches-0996a2cd`. Les fichiers non suivis
du checkout principal et la branche locale `codex/trois-approches` sont conservés.
Tous les essais ci-dessous sont courts, en mémoire ou dans des répertoires
`/tmp` neufs. Aucun processus ou binaire d'un monde hôte n'est consulté,
signalé, relinké ou relancé. Aucun vrai `garde snapshot`, soupe, fuzz ou mix.

## 1. Rendez-vous : remplacer la présence par un tick exécuté

**Défaut confirmé.** La présence de SENSE/LOAD et STEP ne prouve pas leur
exécution : YIELD ou un saut peuvent les rendre inatteignables. Les immédiats
et le point d'entrée courant importent aussi. Un préfiltre statique serait
incomplet : STORE peut créer un STEP exécuté dans ce même tick.

**Diff.** `rendezvous_capable()` conserve l'ancien test pour compatibilité et
ajoute une copie complète du monde accessible à `vm_run`, de toutes ses
créatures vivantes/en attente et du RNG. ATTACK, EAT, STORE, EMIT et SPLIT
n'agissent que sur les copies ; les nouveaux enfants sont libérés. Les nouveaux
accesseurs de `src/vm.c` et `src/world.c` permettent de rétablir l'observateur
VM et les trois compteurs TLS (échelle, somme et nombre de mutations de
naissance). Ils n'ajoutent aucun appel aux voies historiques.

Le nouveau défaut des mondes meeting/all utilise cette porte ; l'option
`--meeting-behavioral 0` choisit l'ancienne. `enabled=0` reste strictement
neutre ; `enabled=1` est l'ancienne porte ; `enabled=2` est le témoin.
Aucun champ ni format de checkpoint n'est ajouté. Une reprise conserve donc
la porte de son checkpoint, comme les autres paramètres.

**Ce qui est prouvé.** Un capteur de la famille antérieure et STEP ont été
exécutés dans le même tick normal, depuis l'état courant avant le guidage.
Ce n'est pas une preuve de dépendance causale capteur→pas, de perception d'une
marque, ni d'amélioration des contacts ou de la survie.

`diagnostic.c` compare les deux portes sur les 16 seeds. Chaque trajectoire
solo passe 64 ticks, plafond 1, nourriture initiale 14 partout, sans repousse.
La dérive indépendante fait 60 générations avec `mutate(.02,.05)` réel ; le
fondateur et chaque mutant sont testés comme nouveau-né dans le même monde.
Les 976 états sous dérive ne sont pas des réplications indépendantes.

| Échantillon | Ancienne porte | Tick témoin | Dénominateur |
|---|---:|---:|---:|
| Seeds initiales | 14 | 14 | 16 |
| États des trajectoires sur 64 ticks | 896 | 744 | 1024 |
| Fondateurs + 60 générations de dérive | 927 | 560 | 976 |

La porte reste largement ouverte sur les fondateurs ; elle ne prétend pas
faire mieux que 14/16 à t=0. Elle distingue les états inactifs pendant la vie
et sous dérive, là où le simple inventaire reste saturé.

Le selftest ajoute un YIELD avant les instructions (statique oui, témoin non),
un STEP créé par STORE (statique non, témoin oui) et un programme qui fait
ATTACK/EAT/STORE/EMIT/SPLIT. Il vérifie par `memcmp` monde, créature, victime,
enfant déjà en attente et RNG, puis compare le contexte TLS et l'observateur.

**Mesure dynamique descriptive**, 8 graines 1..8, profil meeting, 2000 ticks,
1 île 32×16, max-pop=160, pop0=32, regrow=80, stress=1, fondateurs seeds,
sample=200, ck-every=2000, migrate=0. Même ELF neuf
`experiments/rencontres-build/0996-corrections-a01/terrarium`.

| Porte | Contacts cumulés | Lectures | Pas guidés | Temps mural total des 8 runs |
|---|---:|---:|---:|---:|
| statique | 15520 | 76062 | 32100 | 1,052 s |
| témoin | 14909 | 47040 | 26767 | 1,322 s |

Les populations divergent après les premiers guidages ; ces nombres ne sont
pas une estimation causale d'un bénéfice écologique. Le surcoût mesuré est
environ 26 %, dans ce petit protocole. Le clonage peut coûter O(N²) par tick
au pire, s'il y a beaucoup de candidats et de marques. Aucun monde durable
n'est lancé et aucun gain de performance à 4096 adultes n'est revendiqué.

## 2. Nurserie : éprouver le vrai plafond et l'espace local

**Lacune confirmée et comblée par mesure**, sans modifier la loi d'éclosion.
Les runs précédents, plafond 160 sur 512 cases, ne sollicitaient pas le refus
de place. Les fixtures utilisent maintenant le vrai plafond dur `CMAX=4096`,
avec 4095 ou 4096 adultes et un œuf mûr déjà payé. Un tick hors fenêtre de
conception isole son éclosion ; aucune division, mort ou nouvelle dépense.
Les empilements sont autorisés par le moteur : le test sépare le compte global
du remplissage des huit cases du nid. Ce sont des tests aux frontières,
pas des populations apparues par évolution.

| Condition, 32 graines chacune | Adultes | Éclosions | Refus cap | Refus place | Œufs attendent | Nouvelle dépense | Morts |
|---|---:|---:|---:|---:|---:|---:|---:|
| plafond plein, place locale libre | 4096 | 0 | 32 | 0 | 32 | 0 | 0 |
| une place sous cap, voisinage libre | 4095 | 32 | 0 | 0 | 0 | 0 | 0 |
| une place sous cap, voisinage plein | 4095 | 0 | 0 | 32 | 32 | 0 | 0 |
| une place sous cap, une case libre | 4095 | 32 | 0 | 0 | 0 | 0 | 0 |

Les deux refus sont effectifs. Le plafond n'est jamais dépassé, et une case
libérée rend l'éclosion possible sans paiement supplémentaire. **Pas de bug
de physique trouvé**, donc aucune constante n'est changée.

## 3. Deux mondes manquaient aux protections

**Défaut confirmé et corrigé.** Les listes indépendantes de `rencontres.c`
et `garde.c` s'arrêtaient aux douze anciens noms. `mondes.h` devient leur
source commune et ajoute `vie-rencontres-b129054c` et
`vie-rencontres-sonnet-41ab85e`. Le format garde exige 14 lignes et refuse
les anciens inventaires incomplets. Les fixtures utilisent la même liste.

Mesure : 14/14 noms refusés **avant création du répertoire**, plus 2/2 alias
symboliques des nouveaux noms, avec slash final, refusés sans écrire dans
leurs cibles privées `/tmp`. Les 13 fixtures de garde passent, notamment
les listes de 12 et 13 mondes refusées, le doublon, le zombie, le PPID erroné,
le PID réutilisé, l'ELF modifié et le namespace vide. Elles n'accèdent jamais
au vrai `/proc` et ne constituent aucune vérification de l'hôte.

## 4. Sonde-phase : l'énoncé était périmé sur cette base

`origin/main` contient déjà le détecteur v2 (`experiments/sonde_phase.c`) qui
lèse la prochaine cible STORE, au lieu du bloc 16..31 de v1. Le README de
sonde-phase documente aussi ce changement. Aucun diff supplémentaire n'est
justifié. Recompilation dans un chemin neuf, puis rejeu sans fuzz :

| Contrôle | Rejoué | Seuil existant |
|---|---:|---:|
| inerte, bruit, cristal | 0/96 réparations | 0 |
| motif positif, phases fixes | 4/4 | ≥3/4 |
| motif sans lésion | 0/32 | 0 |
| barre des phases réservées | 31/32 | ≥28/32 |

Le selftest compare aussi les deux VM sur 64 bandes. **Sonde valide, défaut
v1 réfuté sur la base actuelle.** Les fichiers historiques restent inchangés.

## Invariants et reproduction

Les dix hashes de `test_core_state_hash_matches_main` sont inchangés, testés
par un pilote ciblé qui appelle aussi les contrôles ISA/seeds et neutralité
du cœur. Il n'exécute pas la suite entière, ni soupe/fuzz/mix.
`src/isa.c`, `src/labs.c`, `src/labs_h.c`, `seeds/` et `tests/test_vm.c`
sont identiques à la base. `Params=416` et l'égalité hooks éteints/contournés
(monde+RNG) sont vérifiés par le selftest rencontres. La reprise 200+200
contre 400 ticks conserve monde, RNG, extensions et les quatre TSV à l'octet.

Depuis la racine, sans `make` nu ni `make test` :

```sh
sh experiments/validation-0996a2cd/verifier.sh
```

Le script crée des chemins neufs, compile directement sans objet partagé,
exécute les selftests, les 128 épreuves de nurserie, les portes et les hashes,
et rejoue la sonde. Il ne lance aucun snapshot réel ou monde durable.
La mesure comparative meeting est descriptive ; sa commande complète est
donnée plus haut. Les sources et résultats des trois nouvelles approches
sont dans leurs propres dossiers. Les sorties brutes restent jointes au
contrat de preuve, sans ELF, TSV ni checkpoint dans Git.
