# general : levier « regs » — 4 registres de lignée (RPUSH / RPEEK)

**Verdict : négatif.** Le levier ouvre des chemins neutres vers une clé de paire (p2 ou p3) en
quatre octets depuis la seed, mais en évolution il n'est pas recruté. Aucun génome sauvé ne passe
le joint m2∧m3 (0/204 génomes vérifiés à 60 cycles sur 3 bras × 4 graines, 0/51 à 300 cycles). Le mur ne bouge pas :
graine 2 à 60 cycles reste exactement à (m2 0.863 ; m3 0.459), min 0.459. C'est le même point,
octet pour octet, que REF60 dans [`general-joint-negative`](../general-joint-negative/README.md).

## Le levier

Le levier n'est actif que dans `general` (`hl_gn_cfg` et `gn_state_new` mettent `regs = GN_REGS`,
1 par défaut). Tous les autres labos gardent `cfg_default().regs == 0`, et 0xF0/0xF1 y restent des
introns. Les seeds ne changent pas, ni les 10 labos historiques, ni le hash de
`test_core_state_hash_matches_main`. Il n'y a pas d'opérateur de naissance.

- `RPUSH k` (0xF0, imm8) écrit `genome[k] = A` exactement comme `STORE`. Il pousse ensuite A dans
  quatre registres de lignée : `r3 = r2 ; r2 = r1 ; r1 = r0 ; r0 = A`.
- `RPEEK k` (0xF1, imm8) fait `A = r[k & 3]`.
- `r[4]` vit dans `Creature`. Il est remis à zéro à la naissance, comme le reste, puis persiste
  d'un essai à l'autre au sein d'une vie. Chaque opcode consomme un pas de budget.

L'idée : la clé de paire de la seed (0x77 : `LOAD 20 ; ADD ; …`) lit la cellule 20 = 32 + 4c du
seul *dernier* essai. Si 0x93 `STORE 20` devient `RPUSH 20`, l'historique des c est gardé. Il
suffit alors de changer 0x77 `LOAD` → `RPEEK` puis l'immédiat 0x78 pour que la clé porte sur c(t−1)
(p2) ou c(t−2) (p3). Chaque marche est à un octet.

## Mesures

`gn_check` sur 40 tâches tenues à l'écart. La ligne « seed » de chaque tableau est la seed
d'origine. Chaque ligne suivante ajoute une mutation à la précédente.

### Chemins à la main (`./experiments/general_regs`)

| génome | m2 | m3 |
|---|---|---|
| seed | 0.366 | 0.401 |
| 0x93 STORE→RPUSH | 0.366 | 0.401 (neutre) |
| + 0x77 LOAD→RPEEK (r0 = c(t)) | 0.366 | 0.401 (neutre) |
| + 0x78 = 0x15 (r1 → clé sur c(t−1), p2) | 0.545 | 0.388 |
| + 0x79 ADD→intron | **0.586** | 0.412 |
| (depuis la marche r0) 0x78 = 0x16 (r2, p3) | 0.385 | 0.537 |
| + 0x79 ADD→intron | 0.399 | **0.562** |

Les deux premières marches sont neutres. La troisième monte de 0.18 sur la mémoire visée. Au-delà,
le chemin plafonne vers 0.59 : l'émission en 0x95 donne priorité à une cellule d'association
confirmée et écrase la table de paires. Pour franchir ce plafond, il faut d'autres octets de
lignée, comme avec unmix. Le levier ne règle pas ce problème-là.

### Évolution, `--no-reload`, graines 0–3

17 génomes vérifiés par course : 16 du pool et le champion.

| bras | cycles | passants m2 | passants m3 | joint | meilleur min(m2,m3) par graine |
|---|---|---|---|---|---|
| Rops = binaire par défaut de la branche (regs + opérateurs 0.12) | 60 | 3+0+0+3 | 2+3+10+10 | **0** | 0.409 / 0.415 / **0.459** / 0.370 |
| REF60 (main, pour mémoire) | 60 | 3+0+0+3 | 10+3+10+10 | 0 | 0.382 / 0.415 / 0.459 / 0.373 |
| Ronly (`-DGN_BIRTH_OFF`, regs seuls) | 60 | 0 | 0 | 0 | 0.434 / 0.491 / 0.477 / 0.489 |
| Base0 (`-DGN_BIRTH_OFF -DGN_REGS=0`, témoin) | 60 | 0 | 0 | 0 | 0.434 / 0.470 / 0.481 / 0.444 |
| Rops, graine 2 | 300 | 11 | 2 | 0 | 0.380 (m2 0.965) |
| Ronly, graines 1, 2 | 300 | 0 | 0 | 0 | 0.488 / 0.509 |

- **Rops ≡ REF60 sur les graines 1 et 2.** Les 17 lignes de `octet-adn general` sont identiques
  à REF60, octet pour octet. Pour ces deux graines, aucun RPUSH ou RPEEK n'a été exécuté dans les
  lignées gardées. Les graines 0 et 3 divergent sans s'approcher du joint : la graine 0 perd même
  8 passants m3.
- **Ronly vs Base0** : le levier seul donne +0.02 à +0.05 de min sur les graines 1 et 3, et rien
  sur les graines 0 et 2. À 300 cycles, cela plafonne vers 0.5. C'est loin des 0.59 atteignables à
  la main, et l'évolution ne trouve même pas le chemin à 3 octets.
- Personne ne passe le joint. Le meilleur min (0.509) vient d'un génome médiocre sur les deux
  mémoires, pas d'un génome à mi-chemin du joint.

Détails et sorties brutes : [`results.txt`](results.txt).

## Reproduire

```sh
make && make test                                        # 38/38
make experiments/general_regs && ./experiments/general_regs  # chemins à la main ; --scan : tous les mutants à 1 octet (> 20 min, non exécuté ici)
for s in 0 1 2 3; do d=/tmp/rops-s$s
  ./octet-adn --labs general --cycles 60 --seed $s --no-reload --data-dir $d >/dev/null
  ./octet-adn general $d/general/checkpoints/latest.ck; done
# bras Ronly / Base0 : make clean && make CFLAGS='-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -DGN_BIRTH_OFF'
#   (Base0 : ajouter -DGN_REGS=0), même boucle
```
