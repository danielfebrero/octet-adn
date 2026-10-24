# Rencontres : essais du 24 octobre 2026, ordre b129054c

Ces observations montrent des chemins supplémentaires de reproduction et de
croisement. Elles ne démontrent ni une émergence, ni une amélioration générale
des rencontres, des sociétés ou de la diversité. La composition `all` ne gagne
pas sur tous les indicateurs.

## Protocole et origine

Ces exécutions ont eu lieu dans le conteneur Codex, dont `/proc` ne voit pas
les douze mondes du VPS. Le checkout initial y était ancien ; un worktree
séparé a été créé depuis `origin/main` à `9da2ed9`. Aucun accès à ces douze
processus n'a été obtenu : le broker a refusé le catalogue faute de scope
`tools:analysis`. L'audit de leurs inodes sur le VPS reste donc à faire ; le
refus de `garde snapshot` dans ce namespace n'est pas une preuve de leur état.

Les 17 essais ci-dessous ont utilisé le même ELF neuf et figé :
`experiments/rencontres-build/b129054c-a02/terrarium`. Les chemins des runs sont
tous nouveaux, sous `experiments/vie-rencontres-b129054c-essais/`. Aucun état
ancien, champ de briques ou checkpoint vivant n'a été copié. Les fondateurs sont
les programmes `seeds` conçus dans le dépôt, et non des organismes apparus du
bruit. Les runs ont été exécutés séquentiellement. Les données de run restent
hors Git ; ce fichier conserve les mesures, sans versionner TSV, ELF ou état.

Pour chacun des profils `control`, `exchange`, `nursery`, `meeting`, `all`, les
graines 11, 29 et 47 ont reçu les mêmes options :

```text
run --dir experiments/vie-rencontres-b129054c-essais/PROFIL-GRAINE
    --profile PROFIL --seed GRAINE --ticks 6000 --islands 2
    --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 80
    --sample 200 --ck-every 2000 --migrate 0 --stress 1 --founders seeds
```

Chaque île commence avec 32 créatures ; le plafond 160 est **par île**, soit
320 pour l'archipel. Chaque run possède 30 fenêtres de 200 ticks et atteint le
tick 6000. Pour les 15 runs, la conservation `population finale = 64 + naissances
− morts` a été vérifiée. Les naissances et morts sont sommées sur les fenêtres ;
les compteurs de `rencontres.tsv` sont cumulatifs et sont lus au dernier sample.

Les réglages des profils dans cet ELF sont : échange de segments entre vivants
tous les 16 ticks, coût 3 par parent et délai de réutilisation de 64 ticks ;
nurserie tous les 16 ticks, coût 24 par parent et 2 unités de nourriture au nid,
incubation 24 ticks, saison de 160 ticks sur 400 et mutation par substitution
de 0,005 ; lecture et déplacement de rendez-vous tous les 4 ticks, coût 1 par
pas. La nurserie réserve un quart du plafond pendant la VM, puis restaure le
vrai plafond pour les éclosions. Elle ne dépasse pas ce plafond et ne provoque
aucune mort pour obtenir une place. Le témoin `control` garde les trois
mécanismes éteints.

## Sens des compteurs

- **Contacts** : observations de paires physiques vivantes, de distance de
  Chebyshev torique au plus 1, une fois par passage de 16 ticks et par paire
  `i < j`. Une même paire peut être observée à plusieurs passages. Ce n'est
  pas un nombre de couples distincts au cours de leur vie, ni une société.
- **Échanges** : paires de vivants dont le segment d'ADN a effectivement changé.
  Ils n'ajoutent pas une créature et n'exigent pas de place sous le plafond.
- **Éclosions croisées** : enfants dont l'ADN recombiné, **avant mutation**,
  différait des deux ADN parentaux. Une mutation de parents identiques ne suffit
  donc pas à incrémenter ce compteur.
- **Sans mort** : éclosions après un `world_step` sans aucune mort sur **la même
  île et le même tick**. Des places ont pu être libérées à un tick antérieur ;
  ce compteur ne prétend pas créer une place au-delà du plafond.
- **Lectures et déplacements** : lectures de marques et pas effectivement payés
  par le guidage. Un grand nombre ne prouve pas une augmentation des contacts.
- **Paires sociales** : observations utilisées par le détecteur social
  historique pendant chaque fenêtre, distinctes du compteur physique ci-dessus.
  Les mêmes individus peuvent contribuer à plusieurs observations.
- **Refus cap/place** : œufs mûrs bloqués à chaque appel, et non œufs détruits.
  **Refus énergie** additionne ceux de la conception et de l'échange. Les refus
  de nourriture, de partenaire ou de cooldown existent dans l'état des modules
  mais ne figurent pas dans ce TSV de synthèse ; ils ne sont pas estimés ici.

## Comparaison des cinq profils : sommes de trois graines

| Profil | Naissances | Morts | Contacts | Échanges | Éclosions | Croisées | Sans mort |
|---|---:|---:|---:|---:|---:|---:|---:|
| control | 38400 | 38338 | 86628 | 0 | 0 | 0 | 0 |
| exchange | 35026 | 34976 | 83893 | 14810 | 0 | 0 | 0 |
| nursery | 37666 | 37557 | 90071 | 0 | 118 | 117 | 39 |
| meeting | 34345 | 34166 | 82778 | 0 | 0 | 0 | 0 |
| all | 35167 | 35034 | 62925 | 13329 | 101 | 99 | 26 |

| Profil | Contacts intra/inter | Octets modifiés par échange | Œufs | Lectures | Déplacements | Œufs en attente |
|---|---:|---:|---:|---:|---:|---:|
| control | 82161 / 4467 | 0 | 0 | 0 | 0 | 0 |
| exchange | 79806 / 4087 | 246026 | 0 | 0 | 0 | 0 |
| nursery | 86271 / 3800 | 0 | 118 | 0 | 0 | 0 |
| meeting | 78430 / 4348 | 0 | 0 | 426818 | 186401 | 0 |
| all | 58820 / 4105 | 222380 | 103 | 304396 | 145670 | 2 |

| Profil | Refus plafond | Refus place | Refus énergie | Refus saison | Échanges au plafond |
|---|---:|---:|---:|---:|---:|
| control | 0 | 0 | 0 | 0 | 0 |
| exchange | 0 | 0 | 790 | 0 | 0 |
| nursery | 0 | 0 | 235 | 1350 | 0 |
| meeting | 0 | 0 | 0 | 0 | 0 |
| all | 0 | 0 | 635 | 1350 | 0 |

La nurserie ouvre bien un chemin vers la naissance qui ne dépend pas d'une mort
du même tick : 39 éclosions de ce type, ou 26 dans `all`. Elle ne fait pas
augmenter le total de naissances par rapport au témoin. `meeting` lit et agit,
mais ses 186401 déplacements accompagnent ici moins de contacts observés que
le témoin. La composition `all` a également moins de contacts. Le nombre
d'échanges au plafond est nul dans ces 15 essais ; un essai distinct plus dense
est donc présenté plus bas.

## Détecteur social historique

| Profil | Paires sociales | Paires intra | Paires inter | Excès max global | Excès max intra | Excès max inter | Événements intra/inter |
|---|---:|---:|---:|---:|---:|---:|---:|
| control | 156302 | 147378 | 8924 | 0,31625 | 0,31485 | 0,18812 | 1 / 0 |
| exchange | 168633 | 160672 | 7961 | 0,22176 | 0,21577 | 0,39624 | 0 / 0 |
| nursery | 113933 | 108498 | 5435 | 0,73692 | 0,72344 | 0,53202 | 2 / 0 |
| meeting | 141698 | 135712 | 5986 | 0,29311 | 0,16525 | 0,14498 | 1 / 0 |
| all | 95574 | 90709 | 4865 | 0,16983 | 0,16107 | 0,25000 | 0 / 0 |

Les maxima sont ceux d'une fenêtre quelconque parmi les trois graines ; ils
ne signifient ni un maintien pendant trois fenêtres, ni un événement. Le seuil
reste paires ≥ 1000 **et** excès > 0,05 pendant trois fenêtres consécutives de
200 ticks, avec publication sur front montant seulement.

Événements réellement inscrits :

| Run | Tick | Type | Excès (bits/paire) | Paires de la fenêtre |
|---|---:|---|---:|---:|
| control-29 | 1600 | societe_intra | 0,2501 | 2000 |
| nursery-29 | 2800 | societe_intra | 0,1390 | 1477 |
| nursery-29 | 4600 | societe_intra | 0,7234 | 1631 |
| meeting-29 | 5800 | societe_intra | 0,0755 | 3634 |

Aucun événement `societe_inter` n'apparaît dans ces 15 runs. Le témoin présente
lui aussi un événement intra ; ces observations ne permettent donc pas
d'attribuer l'apparition de sociétés aux mécanismes ajoutés.

## Dernier sample, par graine

Chaque cellule donne `population / espèces`. L'espèce est le masque de rôle
historique ; ce n'est pas une espèce biologique ni un couple qui se serait
rencontré. Le nombre final de génotypes vaut ici la population dans chacun des
15 runs.

| Profil | Graine 11 | Graine 29 | Graine 47 |
|---|---:|---:|---:|
| control | 51 / 7 | 103 / 2 | 100 / 3 |
| exchange | 36 / 8 | 90 / 2 | 116 / 3 |
| nursery | 46 / 3 | 134 / 3 | 121 / 2 |
| meeting | 124 / 3 | 129 / 4 | 118 / 2 |
| all | 63 / 4 | 128 / 2 | 134 / 1 |

Pour `nursery`, les éclosions/croisées/sans mort sont respectivement
`17/17/8`, `66/65/24`, `35/35/7` aux graines 11, 29, 47. Pour `all`, elles sont
`37/35/12`, `39/39/9`, `25/25/5`.

## Essai supplémentaire : croisement entre vivants au plafond

Deux chemins neufs, `control-plein` et `exchange-plein`, utilisent toujours
l'ELF a02 et des fondateurs `seeds`, sans briques ni checkpoint copié :

```text
run --dir experiments/vie-rencontres-b129054c-essais/PROFIL-plein
    --profile PROFIL --seed 83 --ticks 6000 --islands 1
    --w 16 --h 8 --max-pop 80 --pop0 80 --regrow 200 --osmo 1
    --sample 200 --ck-every 2000 --migrate 0 --stress 1 --founders seeds
```

L'osmotrophie et la nourriture abondante sont un contexte expérimental
supplémentaire, appliqué aux deux runs. Il ne faut pas confondre leurs nombres
bruts avec ceux des archipels précédents.

| Profil | Naissances/morts | Contacts | Échanges | Au plafond | Octets changés | Population finale | Espèces finales |
|---|---:|---:|---:|---:|---:|---:|---:|
| control-plein | 669 / 669 | 156650 | 0 | 0 | 0 | 80 | 6 |
| exchange-plein | 1222 / 1222 | 131248 | 3678 | 3188 | 49994 | 80 | 1 |

Les 3188 échanges au plafond sont des modifications effectives de paires de
vivants, mesurées alors que la population est déjà à 80 avant la VM. Ce chemin
n'exige donc ni une mort, ni une naissance, ni une place supplémentaire. Cela
ne signifie pas 3188 couples distincts au cours de leur vie. Le profil échange
a 25 refus d'énergie. Aucun œuf, éclosion, guidage ou refus de place/plafond
n'est compté dans ces deux profils.

Le témoin est à 80 dans 28 des 30 samples, avec minimum 78 ; le profil échange
dans 29 des 30, avec minimum 74. Les deux conservent exactement
`80 + naissances − morts = 80` et atteignent le tick 6000. Les génotypes finaux
sont 80 dans chaque run.

| Profil | Paires sociales/intra/inter | Excès max global/intra/inter | Événements intra/inter |
|---|---:|---:|---:|
| control-plein | 205762 / 163182 / 42580 | 0,50841 / 0,30890 / 0,51197 | 0 / 1 |
| exchange-plein | 295626 / 283057 / 12569 | 0,23160 / 0,28231 / 0,18532 | 0 / 0 |

L'unique événement est `societe_inter` dans `control-plein`, au tick 1000,
excès 0,5055 bits/paire et 5461 paires dans la troisième fenêtre. L'échange
n'améliore ici ni les contacts physiques, ni le nombre final d'espèces, ni
les événements de société. Son résultat positif précis est de rendre
possible et observé le croisement de vivants au plafond.

## Limites

Trois graines par profil, un horizon de 6000 ticks et une seule graine dense
ne permettent pas une conclusion générale sur la persistance des lignées,
la sélection ou les sociétés à long terme. Les traitements couplent plusieurs
effets : dépense, recombinaison, saison et réserve de population notamment.
Le profil `all` est une combinaison, pas la preuve de leur synergie.

Les ombres et métriques de persistance conservées par le terrarium sont celles
de son protocole historique. Elles ne constituent **pas des témoins adaptés**
aux nouveaux mécanismes d'échange, de nurserie et de guidage ; aucune conclusion
d'émergence n'est tirée de leur comparaison. Les événements sociaux sont les
déclenchements du détecteur défini ci-dessus, pas une validation indépendante
d'une société. Les effets explicitement observés sont les échanges d'ADN, les
naissances avec deux parents et les actions payées à la lecture d'une marque.

## Contestation des choix primaires (Sonnet, ordre 70000966, 24 octobre 2026)

Base `00f1593` (travail Codex b129054c déjà sur `origin/main`). Quatre choix
primaires réexaminés, deux diffs appliqués et mesurés, deux échecs honnêtes
documentés sans changement de code. Tous les runs ci-dessous utilisent le
même ELF neuf, un seul monde de mesure (1 île, 32×16, plafond 160, pop0 32,
regrow 80, `--stress 1 --founders seeds --migrate 0`), jamais un répertoire
protégé. Les sorties brutes des trois balayages restent hors dépôt (TSV dans
`/tmp`, comme pour l'essai b129054c) ; les tableaux ci-dessous sont les
agrégats effectivement lus dans `rencontres.tsv`.

### 1. La fenêtre d'échange ne bouclait pas (corrigé)

Le reste du moteur adresse le génome de façon circulaire (`k % len` pour
`STORE`/`LOAD`, `src/vm.c`). `reciprocal()` dans `echange.c` ne le faisait
pas : `hi = n - locus < 8 ? n - locus : 8` tronquait la fenêtre au lieu de
boucler. Mesure analytique (2 000 000 tirages, `rng_below` réel, n=256) :

| plage du locus | longueur moyenne mesurée | attendu si bouclé (8+1)/2 |
|---|---:|---:|
| 0..247 (248 premiers octets) | 4,501 | 4,5 |
| 248..255 (les 8 derniers) | 2,748 (jusqu'à 1,000 pile sur le dernier octet) | 4,5 |

Corrigé : `i = (locus + k) % n`. Preuve dans `echange_selftest()` : deux
génomes ne différant qu'aux positions 0 et 15 (longueur 16) ; sous l'ancien
code, ces deux positions ne pouvaient **jamais** être échangées ensemble
(locus 0 n'atteint jamais l'index 15 sans dépasser 8 ; locus 15 n'a qu'une
longueur de 1) — `changed` restait ≤ 2. Avec le bouclage, `changed == 4`
apparaît dès qu'un tirage démarre à 15 avec une longueur ≥ 2 (observé sur
l'échantillon de graines du selftest). Les runs de population complets
divergent ensuite de façon chaotique (sensibilité attendue d'un système
évolutif stochastique sur 4000 ticks, pas un signe d'erreur) ; la preuve
retenue est la mesure analytique ci-dessus, indépendante du chaos de
population.

### 2. Le repos d'échange (64 → 32), avec les refus enfin visibles

`EchangeState.cooldown_refused` et plusieurs compteurs de `NaissanceState`
existaient déjà mais n'étaient jamais écrits dans `rencontres.tsv` (limite
documentée plus haut : « ils ne sont pas estimés ici »). Ajoutés :
`exchange_cooldown_refused`, `partner_refused`, `naissance_cooldown_refused`,
`food_refused`, `queue_refused`. Nouveaux drapeaux (`--exchange-period`,
`--exchange-cost`, `--exchange-cooldown`, `--nursery-*`, `--meeting-period`,
`--meeting-cost`, actifs seulement sur un `--dir` neuf) pour les balayer sans
recompiler.

Balayage : profil `exchange`, 8 graines (1-8), 4000 ticks, cooldown ∈
{0, 32, 64, 128, 256} (16 est un no-op strict car la période est 16 : le
premier passage suivant donne toujours `tick - dernier == 16`, jamais `< 16`).
Moyennes des fractions **par contact** (robuste au chaos de population, qui
fait varier `contacts` d'un facteur 50 entre graines à cooldown égal) :

| cooldown | contacts bloqués par le repos seul | taux de réussite (échanges/contacts) |
|---:|---:|---:|
| 0 | 0,0 % | 43,6 % |
| 32 | 42,3 % | 29,0 % |
| **64 (ancien défaut)** | **56,8 %** | **22,3 %** |
| 128 | 69,0 % | 16,7 % |
| 256 | 73,4 % | 14,5 % |

Le défaut `64` refusait plus de la moitié de tous les contacts physiques pour
la seule raison du repos — près de 2,5× le taux à cooldown nul. `32` est la
plus petite valeur qui protège réellement une paire fixe contre sa propre
reconduction à chaque passage (ce que le cooldown est censé faire), tout en
récupérant une bonne partie du taux de réussite perdu à 64. **Nouveau défaut :
32**, posé dans `configuration()`. Vérifié : un run sans drapeau donne des
chiffres identiques, octet pour octet dans `rencontres.tsv`, à un run explicite
`--exchange-cooldown 32` (même graine, même durée).

### 3. La saison de nurserie (160/400) : échec honnête, aucun changement

Balayage : profil `nursery`, 5 graines (1-5), 4000 ticks,
`--nursery-season-open` ∈ {80, 160, 240, 400} (400 = `season_period`, donc
« toujours ouvert »).

| season_open | œufs moyens | éclosions moyennes |
|---:|---:|---:|
| 80 | 6,8 | 6,8 |
| **160 (défaut)** | **16,4** | **16,4** |
| 240 | 24,8 | 24,8 |
| 400 | 31,8 | 31,8 |

Relation linéaire propre (~0,08-0,10 œuf conçu par tick de saison ouverte),
sans palier ni seuil caché. `hatched == eggs` sur toutes les 20 lignes : à
cette densité (plafond 160 sur 512 cases, ~31 %), aucun œuf mûr n'est jamais
refusé par manque de case voisine vide ou par plafond — la limite déjà
documentée plus haut (refus jamais observés dans les essais b129054c) se
confirme ici aussi, y compris en poussant `season_open` jusqu'à 400. Rien à
contester : la constante fixe une fécondité proportionnelle prévisible, pas
un goulot caché. Note indépendante : la graine 4 s'éteint (pop finale 1)
**identiquement** à chacune des quatre valeurs de `season_open` testées,
confirmant que ce n'est pas lié à la nurserie mais à la dynamique générale
de cette graine sur ce monde.

### 4. La porte « capable » des rendez-vous (SENSE + STEP présents) : échec honnête

Hypothèse : la présence statique (n'importe où dans le génome, sans tenir
compte de l'ordre ni de l'exécution réelle) est un portail presque toujours
ouvert. Mesuré hors dépôt (duplication fidèle de `capable()`, jamais commitée) :

- 14 seeds sur 16 du dépôt passent `capable()` dès l'origine, sans mutation.
- Une variante plus stricte (SENSE réel suivi d'un STEP réel à ≤ 16
  instructions, dans l'ordre du programme, sans tenir compte des sauts) donne
  **le même total : 14/16.**
- Sur 60 générations de dérive mutationnelle réelle (`mutate()` du moteur,
  16 lignées issues des seeds, taux 0,02 substitution / 0,05 duplication par
  division — les taux par défaut de `cfg_default()`), les deux portes restent
  saturées entre 14 et 15 sur 16 à chaque génération, sans tendance.

Conclusion : ce n'est pas la position des deux opcodes qui rend la porte
quasi vide, c'est leur fréquence intrinsèque (`LOAD`, qui appartient à la
famille SENSE, et `STEP` sont deux des opcodes les plus communs du jeu). Durcir le
test statique ne change rien de mesurable. Le README documentait déjà cette
porte comme « une loi de reconnaissance du génome, pas une preuve que ces
instructions ont été exécutées ce tick » ; cette mesure confirme que la
reformuler plus strictement, tout en restant statique, ne suffit pas à la
rendre sélective. Aucun code changé pour ce point.

### Validation

`experiments/rencontres-build/sonnet-v4/terrarium selftest` : tous les modules
OK, y compris la nouvelle assertion de bouclage et la reprise identique
(monde, RNG, œufs, cooldowns, marques, et les quatre fichiers TSV octet pour
octet). `Params` toujours 416 octets. `src/`, `include/`, `seeds/` non
touchés ; aucun opcode ajouté. Sanity-check des quatre profils (`control`,
`nursery`, `meeting`, `all`) sur 1000 ticks : aucun crash. Les gardes
existantes (refus `--forces 3`, `--migrate` non nul, répertoires des douze
anciens mondes) répondent toujours par un code de sortie non nul, vérifié
explicitement sur `experiments/vie-tronc` (refusé, rien créé).
