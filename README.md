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
make sanitize     # mêmes tests + 3 cycles sous ASan + UBSan

./octet-adn --dry-run                                  # 1 cycle des 10 labos, rien écrit
./octet-adn --labs all --ticks-per-cycle 200           # runner continu (Ctrl-C ; reprend aux checkpoints)
./octet-adn --labs emit-echo,predict-stock --cycles 50 # quelques labos, 50 cycles
./octet-adn labs                                       # les 10 labos et leur fitness
./octet-adn disasm seeds/forager.adn                   # désassembler un génome
./octet-adn hexdump labs_data/emit-echo/champion.adn   # octets bruts
./octet-adn dump labs_data/predict-stock/metrics.bin   # métriques binaires → texte
./octet-adn ck labs_data/max-pop/checkpoints/latest.ck # inspecter un checkpoint
./octet-adn eval labs_data 8                        # fitness des pools sur 8 graines tenues à l'écart (+ baseline des labos de prédiction)
./octet-adn seeds /tmp/out                             # réexporter les seeds embarquées
```

Options du runner : `--cycles N` (0 = infini), `--data-dir DIR` (défaut `labs_data`),
`--seed N` (graine globale), `--sleep S`, `--select paired|legacy`, `--stock-csv FICHIER` (série locale, voir plus
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

## Les 16 éprouvettes (10 historiques + 6 humanité)

| # | labo | objectif | fitness |
|---|---|---|---|
| 1 | `org-seeds` | organiser d'autres seeds | reçoit les champions des 9 autres labos historiques (crossover) ; lignées distinctes viables (clusters Hamming) × diversité |
| 2 | `max-pop` | population la plus haute | population moyenne + 10 % du pic |
| 3 | `survival` | longévité | espérance de vie moyenne × fraction du cycle survécue (monde pauvre) |
| 4 | `energy-eff` | efficacité métabolique | énergie mangée / créature-tick |
| 5 | `self-rewrite` | auto-réécriture utile | STORE dont le codon réécrit est ensuite exécuté, / créature-tick |
| 6 | `emit-echo` | communiquer | EMIT == stimulus envoyé par le labo ; corrects / créature-tick |
| 7 | `emit-dialog` | protocole | hello(00)→ACK(AC), s→s+1, bye(0F)→00 ; corrects / créature-tick |
| 8 | `predict-walk` | prédire une série AR(2) bruitée | précision sur le bin (0..7) du point suivant ; `baseline` = répéter la dernière valeur, `vs_baseline` = écart (status.md et `eval`) |
| 9 | `predict-stock` | direction d'un « prix » | précision hausse/baisse du pas suivant ; série GBM synthétique ou CSV local |
| 10 | `predator-prey` | écologie | coexistence proies/prédateurs × équilibre des effectifs |

## Les 6 labos « humanité » (11 à 16) — modèles jouets

⚠️ **Ce sont des terrains d'entraînement, pas des percées.** Chaque labo est la miniature d'un vrai
problème, avec une fitness mesurable et une baseline naïve chiffrée (colonne `baseline` de
`status.md` et de `eval`). Un gagnant donne un algorithme à étudier, rien de plus. Les mécanismes
passent par des hooks de labo (`init`, `pre`, `post`, `before_vm`, `ticks_mult`, rejeu
contrefactuel) ; les 10 labos historiques ne les voient pas (chiffres identiques à l'octet près,
vérifié sur graines 0,1,2 × 40 cycles). Code : `src/labs_h.c`.

| # | labo | miniature de | fitness | baseline naïve |
|---|---|---|---|---|
| 11 | `fold` | repliement des protéines (modèle HP 2D) | contacts H-H / optimum connu, moyenne sur 6 benchmarks (Unger & Moult, 20 à 50 résidus) | marche auto-évitante aléatoire |
| 12 | `antibiotic` | antibiothérapie face à la résistance | ticks avant échappement / ticks, ×1.25 si éradiqué, × hôtes vivants, × (1 − dégâts) | même pool sans traitement |
| 13 | `commons` | tragédie des biens communs | pop. moyenne (2e moitié d'un horizon 5×) / 100 × capacité restante | fourrageur glouton |
| 14 | `pandemic` | épidémie SEIR | morts évitées vs même pool sans effet des actions × maintien pop − coût | rejeu sans actions |
| 15 | `code-repair` | réparation automatique de bugs | patients réparés (équivalence sur 256 entrées) / résolus, +0.5 × patch minimal | flips aléatoires sans retour arrière |
| 16 | `self-evolve` | évolvabilité | justesse des EMIT / créature-tick, moyenne sur des régimes de 50 ticks | même pool, paramètres d'évolution fixes |

**fold.** Le génome est rejoué dans un bac à sable : pour placer le résidu *i*, stimulus =
H(i) | H(i+1)<<1 ; `EMIT v` → v%3 = tout droit / gauche / droite. Collision : les autres directions,
sinon repliement tronqué. Score ≤ 1 par construction (optimum connu). Bien plier donne de l'énergie
à la naissance. Seed `fold` : spirale (toujours à gauche) + fourrageur.

**antibiotic.** 40 cellules hôtes immobiles, 12 pathogènes injectés à chaque cycle (jamais dans le
pool : la sélection garderait sinon des pathogènes faibles). La cible d'un pathogène est
`dna[0] & 7`, héritée, et change chez 15 % des nouveau-nés (hypermutation) : la résistance apparaît
*pendant* le cycle. Les agents lisent un antibiogramme (stimulus = cible majoritaire) ; `EMIT v` libère
le médicament v&7 en rayon 1 (coût 2, 1 de dégât par hôte touché).

**commons.** Repousse logistique par case (capacité 14), plus de repousse aléatoire globale. Vider
une case lui retire 4 de capacité **pour toujours**. Seed `commons` : ne mange qu'une case à ≥ 8 et ne
se divise qu'à ≥ 128 d'énergie.

**pandemic.** À t = 20, 8 cas index. I → S voisin : p = 0.3 / tick ; E 3 ticks (sans symptômes) ; I
12 ticks puis mort (30 %) ou guérison. Stimulus propre à chaque créature (hook `before_vm`) :
bit0 malade, bit1 alerte reçue, bit2 immunisé. `EMIT` 1 = alerter (rayon 2), 2 = s'isoler ce tick,
3 = se vacciner (25 d'énergie). La baseline rejoue le même pool sur la même graine, actions sans effet.
Seed : s'isoler quand on est malade.

**code-repair.** Chaque créature reçoit un patient de 8 octets (SENSE_STIM ; MOV_BA ; 2 ops tirées
parmi INC/DEC/ADD/SUB/XOR/AND k ; EMIT ; YIELD), cassé par 1 ou 2 bit-flips. Le patient est mappé sur
`STORE`/`LOAD` imm 0xE0..0xE7 **uniquement si la créature a une fenêtre** (`win_len > 0`, posé par ce
seul labo). Signal : nombre de tests passés sur 16, + 0x40 si mieux que le meilleur vu. La réussite
exige l'équivalence sur les 256 entrées, pas seulement sur les 16 tests. Seed `repair` : recherche
locale bit à bit avec retour arrière (generate-and-validate).

**self-evolve.** Les octets 2..5 du génome sont ses paramètres d'évolution, lus à chaque SPLIT
(`mutate_evo`, uniquement quand `cfg.self_evo`) : substitution ≤ 12 %, indel ≤ 6 %, crossover on/off,
zone protégée ≤ 16 octets et ≤ la moitié du génome. Une cible 0..15 change toutes les 25 ticks (tirée
par la graine du cycle) ; `EMIT v` rapporte 6 − 2·|v − R| si l'écart est < 4, sinon coûte 1. Les
moyennes des paramètres dans la population finale sont dans `status.md` (`mut_rate`, `indel_rate`,
`crossover`, `protected`).

**Exploits testés et fermés** (`test_new_labs_exploits_closed`) : ne rien faire (fold, commons,
code-repair → 0 ; pandemic → ≈ 0 malgré zéro mort, car la population n'est pas maintenue) ; tuer les
hôtes pour affamer le pathogène (→ 0, facteur hôtes vivants) ; arroser des 8 médicaments (dégâts) ;
vacciner ou s'isoler en permanence (coût) ; balayer 0..15 dans self-evolve ; geler l'évolution
(mutation 0 : perd dès que la cible change) ; surapprendre les 16 tests de code-repair (refusé par
l'équivalence sur 256 entrées) ; sélectionner des pathogènes faibles (ils ne sont pas dans le pool).

**Sélection entre cycles (appariée, robuste au bruit)** : après le cycle, le nouveau pool
(les survivants les mieux classés) et le pool en place sont **réévalués sur les mêmes
`SEL_EVALS` = 4 graines** (indépendantes de celle du cycle). Le nouveau pool remplace l'ancien
s'il fait au moins aussi bien en moyenne ; sinon l'ancien pool est retenté avec un autre aléa.
L'ancienne règle (« garder si ≥ 90 % du meilleur historique », qui gelait un labo derrière un
pic chanceux) reste disponible via `--select legacy`. Coût : ~9 cycles simulés par labo et
par cycle (≈ 0,31 s CPU pour les 10 labos historiques, ≈ 0,95 s pour les 16 ; commons tourne sur 5 × ticks et
antibiotic, commons, pandemic, code-repair, self-evolve rejouent une baseline). Les labos de communication / prédiction donnent aussi de l'énergie aux
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

- Les fitness sont bruitées d'un cycle à l'autre ; « meilleure » (affichée) peut être un coup de
  chance. La sélection n'en dépend plus, mais 4 réévaluations restent un échantillon : un pool
  légèrement moins bon peut passer. Mesurer un pool : `octet-adn eval labs_data 8`.
- `emit-echo` est quasi résolu par la seed : le labo mesure surtout la robustesse aux mutations.
- `predict-walk` : la persistance (« répéter la dernière valeur ») fait ≈ 0.332 ; le meilleur prédicteur
  possible à partir des bins (régression vers la moyenne, AR(2) de bruit σ = 1 pour des bins de largeur 1)
  plafonne vers 0.36. La marge battable est donc ≈ +0.03, pas plus. Avant feat/opt-2, la seed `neural`
  lisait ses poids à l'octet 128 : presque chaque enfant les décalait (insertion/délétion avant 128) et
  prédisait au hasard (0.24 dès la seed), puis la sélection par énergie retirait EMIT (runner : 0.02).
  La seed `walk` (poids aux octets 0..4, prédit floor(s/2)+2) et ses descendants font +0.027 sur
  8 graines tenues à l'écart (`eval` affiche baseline et écart).
- Réensemencement : au premier cycle d'un nouveau moteur, si la seed d'un labo bat de plus de 5 % son
  pool en place sur 4 graines, elle le remplace (sinon une seed corrigée n'atteindrait jamais un labo
  déjà lancé).
- `survival` est saturé : la médiane d'âge atteint la durée du cycle (200 ticks), la sélection n'y
  change rien.
- `predator-prey` : les prédateurs (espèce 1) meurent à `PP_PRED_MAX_AGE` = 100 ticks ; ils doivent
  se reproduire pour durer (avant : deux prédateurs « immortels » sans SPLIT figeaient la fitness à
  0.4). Les leviers `pred_metab` et `attack_cost` existent dans `WorldConfig` (0 et 1 par défaut :
  plus durs, ils faisaient chuter la fitness, mesures dans la PR feat/opt-1). Le labo reste instable ; une immigration
  de seeds limite l'extinction.
- Les résultats numériques ne sont pas identiques à l'ancienne version Python (autre RNG) ;
  la sémantique (ISA, monde, fitness, sélection) est portée à l'identique, au plus près.
- Aucune « émergence » au-delà de stratégies simples ; pas de conscience, pas d'autonomie hors de la VM.
- Le moteur est du C compilé, pas de l'assembleur écrit à la main octet par octet : ce qui
  est « octet only », ce sont le produit livré (un ELF), les génomes et toutes les données.
- Labos humanité (40 cycles, graines 0,1,2, eval 8 graines tenues à l'écart) : `fold`, `antibiotic`,
  `commons` et `code-repair` battent nettement leur baseline. **`self-evolve`** : la seed bat les
  paramètres fixes (+0.08), mais après 40 cycles les pools évolués font un peu moins bien que le même
  pool rejoué à paramètres fixes (−0.01 à −0.09) : la population pousse sa zone protégée au maximum
  (16 o), ce qui gèle aussi ses propres paramètres (octets 2..5). **`pandemic`** : très bruité (sd ≈ 0.24
  sur 8 graines, le rejeu contrefactuel diverge), et les pools évolués (≈ 0.02 à 0.08) font moins bien
  que la seed (0.28). Les deux sont des chantiers ouverts, pas des résultats.
