# general : le critère joint m2 ∧ m3 reste hors d'atteinte

Mesure du 22 octobre 2026, moteur de `main` **09fab754c309facfd01e09f39ce6f061e56a9232**.
Cette mesure est un résultat négatif. Sous l'ISA et la sélection actuelles, avec des génomes d'octets
à longueur fixe (172 octets, à un indel de mutation près) :

* la mémoire-1 émerge ;
* la mémoire-2 et la mémoire-3 n'apparaissent que par un échafaudage de naissance (`delay_rate`,
  `unmix_rate`, `deepen_rate`) ou par un transfert entre lignées
  ([general-transfer](../general-transfer/README.md)) ;
* le critère **joint**, m2 ≥ 0,95 **et** m3 ≥ 0,95 sur le même génome, n'est atteint par aucun
  génome évolué.

m2 et m3 désignent la justesse des essais 32 à 63 sur 40 tâches tenues à l'écart. Les passants sont
les génomes du pool (indices 0 à 15 ; le champion, index 16, n'est pas compté) pour lesquels
`octet-adn general latest.ck` annonce une émergence. Toutes les courses durent 200 ticks par cycle
avec `--no-reload`. Les chiffres bruts et les sorties complètes de `octet-adn general` sont dans
[results.txt](results.txt). Les checkpoints restent hors dépôt.

## (a) Null de la sélection directe (piste A)

Question : la mémoire-2 ou la mémoire-3 émerge-t-elle si on la sélectionne directement, sans
échafaudage de naissance ? Le drapeau de compilation `GN_DIRECT_SEL` est intégré à
`src/labs_h.c` et inactif par défaut ; le binaire par défaut est identique, octet pour octet, à
celui de `09fab75`. Une copie du diff est dans [gn-direct-sel.patch](gn-direct-sel.patch). Avec ce
drapeau :

* les taux `delay_rate`, `unmix_rate` et `deepen_rate` sont à 0 ;
* la fitness du pool est la justesse tenue à l'écart de la seule famille `GN_DIRECT_FAM` (6 = T[p2],
  7 = T[p3]) ;
* la sélection garde les 10 meilleurs sur cette mémoire, puis le reste sur l'autre mémoire.

Cette sélection lit le jeu tenu à l'écart lui-même. C'est une fuite volontaire, donc une borne
optimiste : malgré elle, rien ne passe.

```console
make clean && make CC='cc -DGN_DIRECT_SEL -DGN_DIRECT_FAM=6'   # puis =7
./octet-adn --labs general --cycles 60 --seed S --no-reload --data-dir /tmp/oadn-A-m2/sS   # S = 0..3
./octet-adn general /tmp/oadn-A-m2/sS/general/checkpoints/latest.ck
```

| Cible | Graine | Passants m2 / m3 | Meilleur m2 (m3) | Meilleur m3 (m2) | Note finale |
|---|---:|---:|---:|---:|---:|
| m2 (fam 6) | 0 | 0 / 0 | 0,532 (0,475) | 0,523 (0,487) | 0,504 |
| m2 (fam 6) | 1 | 0 / 0 | 0,509 (0,480) | 0,527 (0,476) | 0,493 |
| m2 (fam 6) | 2 | 0 / 0 | 0,538 (0,499) | 0,512 (0,456) | 0,504 |
| m2 (fam 6) | 3 | 0 / 0 | 0,521 (0,513) | 0,527 (0,480) | 0,503 |
| m3 (fam 7) | 0 | 0 / 0 | 0,505 (0,439) | 0,505 (0,502) | 0,468 |
| m3 (fam 7) | 1 | 0 / 0 | 0,513 (0,467) | 0,507 (0,491) | 0,487 |
| m3 (fam 7) | 2 | 0 / 0 | 0,555 (0,540) | 0,540 (0,555) | 0,519 |
| m3 (fam 7) | 3 | 0 / 0 | 0,480 (0,442) | 0,502 (0,418) | 0,452 |

Résultat : **0 passant sur 4 graines**, pour chaque cible. Les cibles plafonnent entre 0,50 et
0,54 (0,555 au maximum, sur la mémoire non ciblée de la graine 2). La seed vaut m2 0,366 et
m3 0,401 ; le hasard vaut 0,25. La sélection directe monte d'environ 0,15, puis s'arrête sur des
tables de paires approchées. Le registre décalé (une dizaine d'octets coordonnés) n'apparaît pas.

## (b) Négatif joint, avec l'échafaudage de naissance

On utilise le binaire par défaut, avec les trois opérateurs de naissance à 0,12.

```console
make clean && make
./octet-adn --labs general --cycles 60  --seed S --no-reload --data-dir /tmp/oadn-ref60/sS   # S = 0..3
./octet-adn --labs general --cycles 300 --seed S --no-reload --data-dir /tmp/oadn-B-long/sS  # S = 0, 2
./octet-adn general /tmp/oadn-ref60/sS/general/checkpoints/latest.ck
```

Le point le plus proche du joint est le génome du pool qui maximise min(m2, m3).

| Cycles | Graine | Passants m2 / m3 | Joint | Plus proche du joint (m2 ; m3) | Note finale (meilleure) |
|---:|---:|---:|---:|---:|---:|
| 60 | 0 | 3 / 10 | 0 | (0,382 ; 0,954) | 0,556 (0,578) |
| 60 | 1 | 0 / 3 | 0 | (0,415 ; 0,588) | 0,495 (0,497) |
| 60 | 2 | 0 / 10 | 0 | **(0,863 ; 0,459)** | 0,558 (0,558) |
| 60 | 3 | 3 / 10 | 0 | (0,373 ; 0,987) | 0,557 (0,560) |
| 300 | 0 | 10 / 2 | 0 | (0,958 ; 0,401) | 0,556 (0,578) |
| 300 | 2 | 11 / 2 | 0 | (0,984 ; 0,384) | 0,557 (0,567) |

L'échange persiste. Un génome tient m2 = 1,00 avec m3 ≈ 0,36, ou m3 ≈ 1,00 avec m2 ≈ 0,36. Sur
300 cycles, le pool garde les deux espèces côte à côte (graine 0 : m2 1,000 / m3 0,361, et
m3 0,999 / m2 0,370), sans recombinaison qui porte les deux. Le point évolué le plus proche du
critère joint est **graine 2, 60 cycles, (0,863 ; 0,459)**. Allonger à 300 cycles ne réduit pas cet
écart : le min retombe à 0,401 et 0,384. Une même clé ne peut pas lire à la fois la case 19 (p2) et
`0x10` (p3), et l'ISA n'a que les registres A et B. Garder les deux demande un second chemin de
clé et une table de plus. À longueur fixe, ce chemin écrase un bloc déjà utilisé. Les solutions
écrites à la main du README racine (255 à 286 octets, sans opcode) ratent elles aussi le joint.

## (c) Déterminisme

On relance deux fois, de façon indépendante, la graine 0 sur 60 cycles avec le binaire par défaut :

```console
./octet-adn --labs general --cycles 60 --seed 0 --no-reload --data-dir /tmp/oadn-B-det/a
./octet-adn --labs general --cycles 60 --seed 0 --no-reload --data-dir /tmp/oadn-B-det/b
cmp /tmp/oadn-B-det/a/general/checkpoints/latest.ck /tmp/oadn-B-det/b/general/checkpoints/latest.ck
```

`cmp` ne signale aucune différence : les checkpoints sont identiques, octet pour octet. On a aussi
reconstruit le binaire `-DGN_DIRECT_SEL -DGN_DIRECT_FAM=6` depuis ce commit et relancé la graine 0
de la piste A. Le checkpoint ne diffère de celui de la course d'origine que par les 8 octets de
l'empreinte moteur (octets 21 à 28, parce que le commentaire du patch a changé). La population
et la sortie de `octet-adn general` sont identiques.

## Reproduire

Depuis la racine du dépôt :

```console
make test                                                     # 37/37, hashes historiques inchangés
make clean && make                                            # (b) et (c)
for S in 0 1 2 3; do ./octet-adn --labs general --cycles 60 --seed $S --no-reload --data-dir /tmp/oadn-ref60/s$S; done
for S in 0 2;     do ./octet-adn --labs general --cycles 300 --seed $S --no-reload --data-dir /tmp/oadn-B-long/s$S; done
make clean && make CC='cc -DGN_DIRECT_SEL -DGN_DIRECT_FAM=6'  # (a), cible m2
for S in 0 1 2 3; do ./octet-adn --labs general --cycles 60 --seed $S --no-reload --data-dir /tmp/oadn-A-m2/s$S; done
make clean && make CC='cc -DGN_DIRECT_SEL -DGN_DIRECT_FAM=7'  # (a), cible m3
for S in 0 1 2 3; do ./octet-adn --labs general --cycles 60 --seed $S --no-reload --data-dir /tmp/oadn-A-m3/s$S; done
for d in /tmp/oadn-*/s*; do ./octet-adn general $d/general/checkpoints/latest.ck; done
make clean && make                                            # revenir au binaire par défaut
```

Durées mesurées sur la box : environ 50 s par course de 60 cycles avec le binaire par défaut, et
environ 160 s avec `GN_DIRECT_SEL`.

Validation : `make test` 37/37. SHA-256 de `seeds/general.adn` :
`b61b2cef91847ce13ee867c4a991acc477a4ac5133fd684e0b92a3f11192d199` (inchangé).
