# 🧬 Octet-ADN — de la vie artificielle écrite en octets

**Octet only.** Un génome est un fichier `.adn` d'octets bruts. Un seul binaire ELF
natif Linux x86_64, `octet-adn` (~60 Ko), les lit octet par octet, les fait vivre,
muter, se réécrire, et fait tourner les 10 éprouvettes. **Pas de Python, pas de JS,
pas d'interpréteur, pas de JSON/YAML** : seeds, checkpoints et métriques sont binaires.
Le moteur est écrit en C + assembleur GNU (`.incbin`), compilé en code machine par `cc`.

> Ceci est une simulation d'évolution, pas une conscience ni une « forme de vie libre » :
> c'est de l'optimisation sous contraintes dans une VM, inspectable octet par octet.

## Build, test, run

Prérequis : `cc` (gcc/clang) et `make`. Aucune autre dépendance (libc + libm).

```bash
make              # → ./octet-adn (ELF x86_64, ~60 Ko, strippé)
make static       # → ./octet-adn-static (glibc statique, ~0,9 Mo, copiable tel quel)
make test         # tests C (tests/test_vm, assert) + garde-fou « no-python »

./octet-adn --dry-run                                  # 1 cycle des 10 labos, rien écrit
./octet-adn --labs all --ticks-per-cycle 200           # runner continu (Ctrl-C ; reprend aux checkpoints)
./octet-adn --labs emit-echo,predict-stock --cycles 50 # quelques labos, 50 cycles
./octet-adn labs                                       # les 10 labos et leur fitness
./octet-adn disasm seeds/forager.adn                   # désassembler un génome
./octet-adn hexdump labs_data/emit-echo/champion.adn   # octets bruts
./octet-adn dump labs_data/predict-stock/metrics.bin   # métriques binaires → texte
./octet-adn ck labs_data/max-pop/checkpoints/latest.ck # inspecter un checkpoint
./octet-adn seeds /tmp/out                             # réexporter les seeds embarquées
```

Options du runner : `--cycles N` (0 = infini), `--data-dir DIR` (défaut `labs_data`),
`--seed N` (graine globale), `--sleep S`, `--stock-csv FICHIER` (série locale, voir plus
bas), `--no-reload`, `--dry-run`.

Vitesse : ~1–15 ms par labo et par cycle de 200 ticks (≈ 50× l'ancienne version Python) ;
40 cycles des 10 labos ≈ 2 s.

## Arborescence

```
seeds/*.adn        génomes de départ, octets bruts (256 o) — la source de vérité
seeds/legacy/      génomes historiques de life.py (ISA 00..0F, 20–25 o)
src/seeds.S        .incbin : les seeds sont copiées telles quelles dans .rodata du binaire
src/vm.c           la VM (le « ribosome »)          src/world.c   monde torique
src/mutate.c       mutations / crossover            src/labs.c    les 10 éprouvettes
src/store.c        checkpoints/métriques binaires    src/runner.c  orchestrateur continu
src/rng.c          RNG déterministe (xoshiro256**)  src/main.c    CLI
include/octet.h    structures + formats binaires    tests/test_vm.c  tests (assert)
```

## Le génome (256 → 1024 octets)

```
0x00..0x7F  code
0x80..0xBF  synapses (blocs de 5 octets int8 : poids food, voisins, énergie, stimulus + biais)
0xC0..0xCF  littéraux lus par LOAD
reste       introns 0xFF (matière première des mutations / duplications)
```

Exemple, `seeds/forager.adn` (`./octet-adn hexdump`) :

```
0d 0a 09 07 0c 07 c0 06 13 08 07 01 0b 0c 07 c1  EAT SENSE JZ+7 STEP LOAD[c0] STORE[13] JMP+7 INC_A TURN STEP LOAD[c1]
06 13 0d 00 0f 1e ff ff ...                      STORE[13] EAT [13]=slot LEV YIELD LOOP introns…
```

L'octet `0x13` est un slot vivant : le génome y écrit `SPLIT` (0e) quand il y a de la
nourriture devant, `NOP` (00) sinon, puis l'exécute — il réécrit son comportement.

## ISA

1 octet = 1 codon ; `*` = suivi d'un imm8. `00..0F` = ISA historique (compat `life.py`),
`10..1F` = extension, `20..FF` = introns (lus gratuitement, sans effet). 16 ops / tick.

| op | nom | effet | op | nom | effet |
|---|---|---|---|---|---|
| 00 | NOP | intron | 10 | SENSE_NB | A = nb de voisins |
| 01/02 | INC_A/DEC_A | A±1 | 11 | SENSE_EN | A = énergie / 8 |
| 03/04 | INC_B/DEC_B | B±1 | 12 | SENSE_STIM | A = stimulus externe |
| 05 | CLR_A | A = 0 | 13* | NEURAL | A = neurone(4 capteurs × synapses[imm]) |
| 06* | STORE | genome[imm] = A ★ | 14 | EMIT | émet A (réponse, 1×/tick) |
| 07* | LOAD | A = genome[imm] | 15/16 | MOV_BA/MOV_AB | B=A / A=B |
| 08* | JMP | ip += imm | 17/18/19 | ADD/SUB/XOR | A op= B |
| 09* | JZ | si A==0 : ip += imm | 1A | ATTACK | vole de l'énergie à un voisin d'une autre espèce |
| 0A | SENSE | A = nourriture devant | 1B* | JNZ | si A≠0 : ip += imm |
| 0B | TURN | direction = A % 4 | 1C* | LOADI | A = imm |
| 0C | STEP | avance (−1 énergie) | 1D* | SENSE_HIST | A = stimulus[t−1−imm] |
| 0D | EAT | mange | 1E | LOOP | ip = 0 |
| 0E | SPLIT | division + mutation | 1F* | AND | A &= imm |
| 0F | YIELD | fin du tick | | | |

**NEURAL** : `A = clamp(0..255, ⌊(Σ wᵢ·xᵢ + 8) / 16⌋ + biais)`, entrées 0..15
(nourriture devant, 2×voisins, énergie/16, stimulus&15), poids int8 lus dans le génome.

**Mutations** à la division : substitution par un codon, substitution brute, micro-dérive
±1/±3 (poids), insertion, délétion, duplication de bloc (2–16 o) ; crossover optionnel.

**Déterminisme** : tout l'aléa vient d'un RNG xoshiro256** amorcé par
`FNV-1a("labo:cycle:seed")`. Même seed → même monde, même `state_hash` (testé).

## Les 10 éprouvettes

| # | labo | objectif | fitness |
|---|---|---|---|
| 1 | `org-seeds` | organiser d'autres seeds | reçoit les champions des 9 autres labos (crossover) ; lignées distinctes viables (clusters Hamming) × diversité |
| 2 | `max-pop` | population la plus haute | population moyenne + 10 % du pic |
| 3 | `survival` | longévité | espérance de vie moyenne × fraction du cycle survécue (monde pauvre) |
| 4 | `energy-eff` | efficacité métabolique | énergie mangée / créature-tick |
| 5 | `self-rewrite` | auto-réécriture utile | STORE dont le codon réécrit est ensuite exécuté, / créature-tick |
| 6 | `emit-echo` | communiquer | EMIT == stimulus envoyé par le labo ; corrects / créature-tick |
| 7 | `emit-dialog` | protocole | hello(00)→ACK(AC), s→s+1, bye(0F)→00 ; corrects / créature-tick |
| 8 | `predict-walk` | prédire une série AR(2) bruitée | précision sur le bin (0..7) du point suivant (baseline persistance affichée) |
| 9 | `predict-stock` | direction d'un « prix » | précision hausse/baisse du pas suivant ; série GBM synthétique ou CSV local |
| 10 | `predator-prey` | écologie | coexistence proies/prédateurs × équilibre des effectifs |

**Sélection entre cycles** : le pool (16 génomes) est remplacé par les survivants les mieux
classés si la fitness du cycle ≥ 90 % de la meilleure ; sinon l'ancien pool est retenté
avec un autre aléa. Les labos de communication / prédiction donnent aussi de l'énergie aux
bonnes réponses : la sélection naturelle agit dans le monde.

**La « popote » partagée** : les 10 labos tournent dans le même binaire. Si on recompile
`octet-adn` pendant que le runner tourne, il le détecte au cycle suivant et se ré-exécute
(`execv`) en reprenant les checkpoints : l'amélioration de la VM/ISA s'applique à tous les
labos. L'empreinte du binaire (`moteur`) est enregistrée dans chaque checkpoint et métrique.

## Fichiers produits (tous binaires sauf status.md)

```
labs_data/<labo>/checkpoints/latest.ck   OADNCKP1 | u8 labo | u32 cycle | f64 best | u64 moteur
                                         | u8 has_best | [u8 espèce, u16 len, octets] | u16 n | n × génome
labs_data/<labo>/champion.adn            meilleur génome, octets bruts
labs_data/<labo>/metrics.bin             OADNMET1 + 1 enregistrement fixe (MetricRec, include/octet.h) par cycle
labs_data/status.md                      tableau de bord lisible (seul fichier texte)
```

Un exemple est commité dans `labs_data/sample/` (40 cycles, `--seed 0`).

## ⚠️ predict-stock : disclaimer

**Ce n'est pas un modèle de trading et ce n'est pas un conseil financier.** La série par
défaut est un mouvement brownien géométrique : les rendements sont indépendants, donc la
direction du pas suivant est **imprévisible par construction** et ~50 % est le plafond
attendu. Toute précision au-dessus vient du bruit d'échantillonnage. Le labo sert à montrer
qu'un système évolutif ne crée pas d'information là où il n'y en a pas. Une série locale
peut remplacer le GBM : `--stock-csv prix.csv` (entête avec une colonne `close`). **Aucun
appel réseau.**

## Limites honnêtes

- Les fitness sont bruitées d'un cycle à l'autre ; « meilleure » peut être un coup de chance,
  et la règle des 90 % peut bloquer un labo sur un ancien pool pendant longtemps.
- `emit-echo` est quasi résolu par la seed : le labo mesure surtout la robustesse aux mutations.
- `predict-walk` : les génomes font rarement mieux que la baseline de persistance.
- `predator-prey` est instable (effondrements, plateaux) ; une immigration de seeds limite
  l'extinction mais ne garantit pas l'équilibre.
- Les résultats numériques ne sont pas identiques à l'ancienne version Python (autre RNG) ;
  la sémantique (ISA, monde, fitness, sélection) est portée à l'identique, au plus près.
- Aucune « émergence » au-delà de stratégies simples ; pas de conscience, pas d'autonomie hors de la VM.
- Le moteur est du C compilé, pas de l'assembleur écrit à la main octet par octet : ce qui
  est « octet only », ce sont le produit livré (un ELF), les génomes et toutes les données.
