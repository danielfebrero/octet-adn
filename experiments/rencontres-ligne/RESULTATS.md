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
