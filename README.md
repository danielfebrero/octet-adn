# 🧬 Octet-ADN — de la vie artificielle écrite en octets

Un **génome est une suite d'octets**. Chaque octet est un codon que lit une petite
machine virtuelle déterministe (le « ribosome »). Les créatures mangent, se déplacent,
se divisent avec mutations, **réécrivent leur propre ADN** (`STORE`), perçoivent un
**stimulus envoyé par nous**, calculent avec des **synapses** stockées dans le génome
(`NEURAL`) et **répondent** (`EMIT`).

Dix **éprouvettes** (labos) font évoluer leur propre pool de génomes, chacune avec un
objectif différent, en continu. Toutes partagent le même moteur : améliorer la VM ou
l'ISA (la « popote ») profite à tous les labos dès le cycle suivant (hot-reload).

> Ceci est une simulation d'évolution, pas une conscience ni une « forme de vie libre » :
> c'est de l'optimisation sous contraintes dans une VM, inspectable octet par octet.

## ADN biologique vs Octet-ADN

| Capacité | ADN (vie) | Octet-ADN |
|---|---|---|
| Alphabet | 4 bases (A/C/G/T) | 256 valeurs d'octet |
| Codon | triplet → acide aminé | 1 octet → 1 instruction (+ imm8 éventuel) |
| Interpréteur | ribosome | `octet_adn/vm.py` (16 ops/tick) |
| Introns | ADN non codant | octets hors ISA : lus gratuitement, sans effet |
| Régulation | épigénétique, facteurs de transcription | `STORE` réécrit un codon pendant la vie |
| Mutation | erreurs de copie | substitution, micro-dérive, insertion, délétion, **duplication de bloc** |
| Recombinaison | méiose | crossover optionnel à la division |
| Système nerveux | neurones | `NEURAL` : 4 capteurs × 4 poids int8 + biais, stockés dans le génome |
| Communication | signaux chimiques, son | `SENSE_STIM` (entrée) / `EMIT` (sortie) |
| Taille | Mb → Gb | 256 octets au départ, jusqu'à 1024 |

## Le génome (256 → 1024 octets)

```
0x00..0x7F  code
0x80..0xBF  synapses (blocs de 5 octets int8 : poids food, voisins, énergie, stimulus + biais)
0xC0..0xCF  littéraux lus par LOAD
reste       introns 0xFF (matière première des mutations / duplications)
```

## ISA

`00..0F` = ISA historique (identique à `life.py`), `10..1F` = extension. `*` = prend un imm8.

| op | nom | effet | op | nom | effet |
|---|---|---|---|---|---|
| 00 | NOP | intron | 10 | SENSE_NB | A = nb de voisins |
| 01/02 | INC_A/DEC_A | | 11 | SENSE_EN | A = énergie // 8 |
| 03/04 | INC_B/DEC_B | | 12 | SENSE_STIM | A = stimulus externe |
| 05 | CLR_A | A = 0 | 13* | NEURAL | A = neurone(capteurs × synapses[imm]) |
| 06* | STORE | genome[imm] = A ★ | 14 | EMIT | émet A (réponse) |
| 07* | LOAD | A = genome[imm] | 15/16 | MOV_BA/MOV_AB | B=A / A=B |
| 08* | JMP | ip += imm | 17/18/19 | ADD/SUB/XOR | A op= B |
| 09* | JZ | si A==0 : ip += imm | 1A | ATTACK | vole de l'énergie à un voisin d'une autre espèce |
| 0A | SENSE | A = nourriture devant | 1B* | JNZ | si A≠0 : ip += imm |
| 0B | TURN | direction = A % 4 | 1C* | LOADI | A = imm |
| 0C | STEP | avance (−1 énergie) | 1D* | SENSE_HIST | A = stimulus[t−1−imm] |
| 0D | EAT | mange | 1E | LOOP | ip = 0 |
| 0E | SPLIT | division + mutation | 1F* | AND | A &= imm |
| 0F | YIELD | fin du tick | | | |

La VM est **déterministe** : pour un même seed RNG, même monde, même état (testé).

## Les 10 éprouvettes

| labo | objectif | fitness |
|---|---|---|
| `org-seeds` | organiser d'autres seeds | reçoit les champions des 9 autres labos, crossover ; lignées distinctes viables (clusters Hamming) × diversité |
| `max-pop` | population la plus haute | population moyenne + 10 % du pic |
| `survival` | longévité | espérance de vie moyenne × fraction du cycle survécue (monde pauvre) |
| `energy-eff` | efficacité métabolique | énergie mangée / créature-tick |
| `self-rewrite` | auto-réécriture utile | STORE dont le codon réécrit est ensuite exécuté, / créature-tick |
| `emit-echo` | communiquer | EMIT == stimulus envoyé par le labo ; corrects / créature-tick |
| `emit-dialog` | protocole | hello(00)→ACK(AC), s→s+1, bye(0F)→00 ; corrects / créature-tick |
| `predict-walk` | prédire une série AR(2) bruitée | précision sur le bin (0..7) du point suivant (baseline persistance affichée) |
| `predict-stock` | direction d'un « prix » | précision hausse/baisse du pas suivant ; série GBM synthétique ou CSV local |
| `predator-prey` | écologie | coexistence proies/prédateurs × équilibre des effectifs |

Chaque labo a : son monde (config), sa fitness, sa seed (`seeds/`), son checkpoint
(`labs_data/<labo>/checkpoints/latest.json`), son champion (`champion.adn`) et ses
métriques (`labs_data/<labo>/metrics.jsonl`).

**Sélection entre cycles** : le pool (16 génomes) est remplacé par les survivants les
mieux classés si la fitness du cycle ≥ 90 % de la meilleure ; sinon l'ancien pool est
retenté au cycle suivant avec un autre aléa. Les labos de communication / prédiction
donnent aussi de l'énergie aux bonnes réponses : la sélection naturelle agit dans le monde.

## Lancer

```bash
# tests (stdlib uniquement)
python3 -m unittest discover -s tests -t . -v

# un cycle sans rien écrire
python3 -m octet_adn.runner --labs all --ticks-per-cycle 200 --dry-run

# runner continu (Ctrl-C pour arrêter, reprend depuis les checkpoints)
python3 -m octet_adn.runner --labs all --ticks-per-cycle 200

# quelques labos, 50 cycles
python3 -m octet_adn.runner --labs emit-echo,predict-stock --cycles 50
```

Tableau de bord : `labs_data/status.md` (régénéré à chaque cycle, avec la tendance de
fitness et l'empreinte `moteur` du code). Un exemple est commité dans `labs_data/sample/`.

Le script historique `life.py` (ISA 00..0F, génome 25 octets) est conservé tel quel.

## ⚠️ predict-stock : disclaimer

**Ce n'est pas un modèle de trading et ce n'est pas un conseil financier.** La série par
défaut est un mouvement brownien géométrique : les rendements sont indépendants, donc la
direction du pas suivant est **imprévisible par construction** et ~50 % est le plafond
attendu. Toute précision au-dessus vient du bruit d'échantillonnage. Le labo sert à
montrer qu'un système évolutif ne crée pas d'information là où il n'y en a pas. Un CSV
local (colonne `close`) peut remplacer la série (`PredictStock.csv_path`) ; aucun appel réseau.

## Limites honnêtes

- Simulation Python pure : ~0,1–0,3 s par labo et par cycle de 200 ticks, mondes 48×20, ≤ 120 créatures.
- Les fitness sont bruitées d'un cycle à l'autre ; « meilleure » peut être un coup de chance.
- `emit-echo` est quasi résolu par la seed : le labo mesure surtout la robustesse aux mutations.
- `predict-walk` : les génomes font rarement mieux que la baseline de persistance.
- Aucune « émergence » au-delà de stratégies simples ; pas de conscience, pas d'autonomie hors de la VM.
