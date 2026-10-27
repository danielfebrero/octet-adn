# 🧬 Octet-ADN — de la vie artificielle écrite en octets

[**Relief : le nouveau monde**](experiments/relief-20261025/README.md).

[**Transmission : conserver et nous écrire, monde distinct vivant**](experiments/transmission-20261026/README.md).

[**Les 34 anciens : archives et fiches**](ops/mondes/MONDES.md).
[**Archivage, preuves et limites**](ops/archives/20261025/README.md).

[**Faits remarquables : observations, preuves et limites par monde**](ops/mondes/FAITS-REMARQUABLES.md).

Expériences isolées de l'ordre 0996a2cd (aucun changement d'ISA) :
[pari phénotypique](experiments/pari-phenotypique/README.md),
[symbiose par paquets](experiments/symbiose-paquets/README.md),
[héritage du milieu](experiments/heritage-milieu/README.md).
Les pré-enregistrements précèdent les comptes ; chaque dossier contient son
C, son script de build neuf et son verdict. [Corrections et validation](experiments/validation-0996a2cd/README.md)
documente le tick témoin des rendez-vous, les quatorze noms protégés, la
nurserie au plafond réel et le rejeu de la sonde déjà corrigée.

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
./octet-adn general labs_data/general/checkpoints/latest.ck # general : chaque génome rejoué seul (mémoire 2 et 3, gelé, sans récompense)
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

## Les 19 éprouvettes (10 historiques + 7 humanité + general + dialog-mem)

| # | labo | objectif | fitness |
|---|---|---|---|
| 1 | `org-seeds` | organiser d'autres seeds | reçoit les champions des 9 autres labos historiques (les 8 premiers utilisés) ; le génome du pool humanité au plus grand `vs_baseline` > 0 remplace le 8e ; lignées distinctes viables (clusters Hamming) × diversité |
| 2 | `max-pop` | population la plus haute | population moyenne + 10 % du pic |
| 3 | `survival` | longévité | espérance de vie moyenne × fraction du cycle survécue (monde pauvre) |
| 4 | `energy-eff` | efficacité métabolique | énergie mangée / créature-tick |
| 5 | `self-rewrite` | auto-réécriture utile | STORE dont le codon réécrit est ensuite exécuté, / créature-tick |
| 6 | `emit-echo` | communiquer | EMIT == stimulus envoyé par le labo ; corrects / créature-tick |
| 7 | `emit-dialog` | protocole | hello(00)→ACK(AC), s→s+1, bye(0F)→00 ; corrects / créature-tick |
| 8 | `predict-walk` | prédire une série AR(2) bruitée | précision sur le bin (0..7) du point suivant ; `baseline` = répéter la dernière valeur, `vs_baseline` = écart (status.md et `eval`) |
| 9 | `predict-stock` | direction d'un « prix » | précision hausse/baisse du pas suivant ; série GBM synthétique ou CSV local |
| 10 | `predator-prey` | écologie | coexistence proies/prédateurs × équilibre des effectifs |

## Les 7 labos « humanité » (11 à 17) — modèles jouets

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
| 16 | `self-evolve` | évolvabilité | justesse des EMIT / créature-tick, moyenne sur des régimes de 50 ticks, 4 paires | même pool, paramètres d'évolution fixes |
| 17 | `diagnose` | calculer une réponse qui n'est pas dans le capteur | justesse des EMIT : médicament = ((marque + 1) XOR 4) mod 8, marque = stimulus | même pool, stimulus renversé (`marque XOR 7`) |
| 18 | `general` | apprendre pendant sa vie, généraliser | justesse des essais 32..63 sur la **mémoire 2 et la mémoire 3** tenues à l'écart (fitness et best affichées) ; pool en deux moitiés : niche mémoire (échelle T[p1], T[p2], T[p3]) et curriculum ; la réévaluation appariée note les deux | mêmes génomes, génome gelé (`frozen`, `vs_frozen`) ; hasard 0.25 ; constante oracle 0.35 |
| 19 | `dialog-mem` | dialogue qui exige une mémoire | crédit gradué (Hamming 4 bits) / créature-tick, réponse = (s_{t-1} + 1) mod 16, historique vidé | même pool, faux s_{t-1} (`baseline`, `vs_baseline`) ; plafond sans mémoire 0,2875 |

**dialog-mem.** emit-dialog est atteignable par une politique figée (s → s+1) : rien à faire émerger. dialog-mem demande de répondre au message d'AVANT, (s_{t-1} + 1) mod 16, sans historique : toute politique sans mémoire a la même espérance (1/16 exact, crédit 0,2875 ; Monte-Carlo 10^6 ticks : table s+1 0,2878, 2000 tables aléatoires 0,2875 en moyenne, max 0,2884 ; avec mémoire 1,0000). Labo ajouté en fin de registre (index 18) : les 18 autres labos sont identiques à l'octet (dumps metrics.bin, state_hash inclus, 3 cycles graine 7). La seed-amorce est celle d'emit-dialog, sans nouveau fichier. Sur 16 graines tenues à l'écart (`octet-adn eval`) : seed 0,282 (exact 0,119). Après 300 cycles, graines 1 à 4 : 0,950 / 0,939 / 0,940 / 0,802, baseline faux s_{t-1} 0,270 / 0,262 / 0,266 / 0,228. Le champion émet en début de tick la valeur gardée dans A (s_{t-1} + 1), puis relit le stimulus et incrémente : plusieurs mutations coordonnées depuis la seed. Cible somme s_t + s_{t-1} essayée : pas d'émergence (≤ 0,34).

**general.** Stimulus = indice (bits 0-1) | récompense affichée de l'essai précédent (bit 2) | début de tâche (bit 3) ; réponse = `EMIT v` (v & 3). Entraînement : association, renversements (1 à 3, instants aléatoires), bandit (non stationnaire), paire T[indice, indice précédent], XOR, récompense bruitée 10-20 % ; le niveau monte quand le pool dépasse 0.9. Un quart des vies du monde est une mémoire longue (tables non réservées). La seed (172 octets) écrit une table par indice et une table par paire, remises à zéro au début de tâche. Elle généralise ces familles à des tables réservées (colonne `known`). Elle ne retient pas l'indice d'il y a 2 essais, ni celui d'il y a 3. La note affichée est uniquement ces deux mémoires, sur des tables réservées : une table sur l'indice courant y vaut le hasard, parce que la cible ne dépend pas de lui. Le pool a deux moitiés. Places 0-7 : la niche mémoire, trois places sur T[p1], trois sur T[p2], deux sur T[p3] (autres tables que le jeu tenu à l'écart) ; la note affichée et le champion viennent d'elle. Places 8-15 : la sélection d'avant, moitié curriculum, moitié mémoire 2 et 3. La réévaluation appariée note les deux moitiés (moitié échelle de la niche, moitié sélection du curriculum). Un programme à ligne à retard, absent de la seed, confirme `T[décalage]` quand la récompense dit juste et incrémente sa conjecture sinon : il est exact sur la seconde moitié de vie, et retombe si on gèle le génome ou si on masque la récompense. Ce labo seul : 192 instructions/tick, mutation 0.5 %, fitness = le pool lui-même en bac à sable, sélection génome par génome. Hérédité : l'enfant et le pool suivant reçoivent le génome de naissance. Les STORE de la vie restent dans le corps et ne deviennent pas des gènes. Les copies de la même lignée ne prennent qu'une place parmi les candidats (toutes les lignées du monde, au plus 96). Les autres labos ne posent pas ce drapeau. Le mélange tenu à l'écart de PR #9 (0,73 pour la seed, mémoire-2 noyée à 0,38) n'est plus la note. Mesuré : seed mémoire-2 0,366, mémoire-3 0,401, familles connues 0,726, sélection du pool 0,449. Ligne à retard 0,995 et 0,979, gelée 0,299 et 0,294, sans récompense 0,238 et 0,252. Course graine 0, 60 cycles, avec cette hérédité : la note reste entre 0,383 et 0,390 (meilleur 0,390), population 82–120. Les 16 génomes sont distincts, à 9–21 octets de la seed, et l'écart est surtout dans le code. La ligne à retard n'y est pas.

**general : l'échelle mémoire.** Pourquoi la ligne à retard n'émergeait pas (mesures hors labo, mêmes fonctions de note). Aucun mutant à un octet de la seed ne fait mieux qu'elle en sélection (0,544 ; meilleur mutant 0,550). Le meilleur mutant pour le lag fait 0,505 en payant le curriculum. Une évolution hors ligne (16 parents, 64 enfants, 3 000 générations, opérateur `mutate` du labo) reste à mémoire-2 ≤ 0,43 avec la sélection du labo, et ≤ 0,533 en sélectionnant sur le lag seul. Ce sont des bricolages, pas des registres. Une marge neutre de 64 NOP en queue de seed ne change rien. Sur un tiers des paires de substitutions dans le code (premier octet de 75 à 106), la meilleure tient la mémoire-2 à 0,631. Retenir p2 demande un registre de plus : copier l'indice avant qu'il soit écrasé, soit une dizaine d'octets coordonnés. La seed apprend mal T[p1] (0,53), par sa table de paires. Un registre la tient à 0,93–1,0, mais coûte l'association (1,0 → 0,39) : la sélection d'avant jetait chaque maillon. D'où la niche : la moitié du pool est choisie sur l'échelle T[p1], T[p2], T[p3], où l'association ne pèse pas. Hors labo, sélection sur l'échelle seule, depuis la seed : mémoire-1 à 0,969–0,999 sur 4 graines sur 4. Depuis un génome à un registre (hors seed, jamais commité), un second maillon donne la mémoire-2 sur 2 graines sur 4 (0,998, gelé 0,216, sans récompense 0,245). Dans le labo, graines 0 à 3, 2 000 cycles. Avec la niche : la tête de niche tient seule la mémoire-1 sur les tables réservées (0,983–0,998 ; gelée 0,168–0,302 ; sans récompense 0,245–0,252 ; seed 0,565). Meilleure note 0,455 / 0,447 / 0,495 / 0,461. Sans niche (main 9b56a38), mêmes conditions : mémoire-1 0,549–0,665, meilleure note 0,397 / 0,436 / 0,395 / 0,443. La note peut descendre sous le hasard un cycle (0,178 une fois, graine 1), car la niche n'est pas choisie sur elle. Aucun génome sauvé ne passe `octet-adn general` : pas d'émergence de la mémoire 2 ou 3. Le premier maillon trouvé par le labo passe par la table de paires (la clé ne lit plus l'indice courant). Prolongée 7 000 générations hors ligne, cette voie n'atteint pas la mémoire-2 (≤ 0,46). Le maillon qui y mène écrit l'indice dans un octet libre en fin d'essai.

**Le second barreau, sans allonger le génome.** Une insertion décale les STORE absolus et fait tomber la mémoire-1 (quatre NOP au milieu : 0,945 → 0,445). À longueur égale, l'idiome qui écrit l'indice dans la case 20 peut d'abord copier l'ancienne valeur dans la case 19. Mesuré sur 40 tâches, à partir du registre déjà connu (LOAD 20 à la place de SENSE_STIM, hors seed) : mémoire-1 0,945 → 1,000, mémoire-2 inchangée (0,36). Relire la case 19 au lieu de la case 20 : mémoire-2 0,386 → 0,511, mémoire-1 0,434. La moyenne des trois barreaux classe ce génome sous le registre parfait. L'échelle ne moyenne plus : un barreau ≥ 0,42 qui dépasse le précédent vaut 1 ou 2 de plus que sa justesse, et la note d'un pool est la moyenne de ces échelles, pas l'échelle de la moyenne. À la naissance, ce labo seul peut appliquer les deux réécritures (`delay_rate` 0,12 ; les autres labos ne tirent pas ce hasard). Hors labo, relire la case 19 donne 0,51 : sous le critère. Rien de cette ligne n'est écrit dans `seeds/general.adn`.

**Course graine 0, 60 cycles, avec ce barreau.** La note affichée (moyenne des mémoires 2 et 3 de la niche) part de 0,384 et finit à 0,578. Population finale 74. Familles connues 0,726. Onze génomes du pool, toujours 172 octets, à 14–17 octets de la seed, passent `octet-adn general` sur la mémoire-2 seule : 1,000 sur 40 tâches tenues à l'écart. Le plus proche (14 octets) : gelé 0,188, sans récompense 0,252, mémoire-3 0,359. Trois écarts portent cette note, les autres sont du bruit de la lignée. Remettre `LOAD 20` à la place de `LOAD 19` (la case que le décalage vient d'écrire) fait tomber la mémoire-2 à 0,364. Remettre le masque d'émission `AND 4` à la place de `AND 184` la fait tomber à 0,349. Remettre la constante de confirmation 36 à la place de 56 la fait tomber à 0,612. Le champion sauvé ne passe pas (mémoire-2 0,437) : il est choisi sur la moyenne des deux mémoires, il a le décalage, il n'a pas la relecture de la case 19. La mémoire-3 ne passe pas.

**Quatre graines, 60 cycles.** Même binaire. Graine 0 : 11 génomes, mémoire-2 à 1,000, note 0,578. Graine 1 : 3 génomes, 0,996 à 0,998, note 0,544. Graine 3 : 6 génomes, 0,965 à 1,000, note 0,543 (meilleure 0,553), familles connues jusqu'à 0,816. Graine 2 : aucun, le meilleur reste à 0,530, note 0,413. Partout le champion affiché rate le critère, et la mémoire-3 reste sous 0,44. Les génomes qui passent sur les graines 1 et 3 ont le décalage et le `LOAD 19`, sans la constante 56 ni le masque 184 de la graine 0. La graine 2 a ce même `LOAD 19` sur ses meilleurs génomes et s'arrête à 0,530. Longueur 172 partout.

`octet-adn general FICHIER.adn|.ck` rejoue chaque génome seul. Il ne déclare une émergence que si la mémoire 2 ou 3 tient ≥ 0,95 seule, et retombe d'au moins 0,5 gelée ET sans récompense. Code retour 0 si oui, 3 sinon. Les lignes à retard des tests passent (0,995 et 0,979), la seed non.

**Transfert minimal entre lignées (40 tâches).** Sur le génome du pool 3 de la graine 2,
bloqué à mémoire-2 0,530 après 60 cycles, un seul octet du pool 3 de la graine 3 suffit :
`0x79 : 17 → cf` (ADD remplacé par un intron existant). Mémoire-2 0,987, gelée 0,260,
sans récompense 0,266 ; mémoire-3 0,354 ; known 0,798. `octet-adn general` retourne 0.
Le génome fait toujours 172 octets, à 21 substitutions de la seed. Les 36 transferts
d'un octet disponibles depuis un passant de la graine 0 et un de la graine 3 ont été
mesurés : celui-ci seul passe. L'ensemble vide échoue, donc un octet est minimal sur
cette base. Ce résultat combine les lignées 3 et 2 ; il ne mesure pas une nouvelle
course évolutive. Arrêt à ce premier fait nouveau, sans opérateur supplémentaire ni
modification de la seed. [Entrées, différences et protocole reproductible](experiments/general-transfer/README.md).

**Le négatif joint.** Aucun génome évolué ne tient m2 ∧ m3 ≥ 0,95 (meilleur point : graine 2, (0,863 ; 0,459)), et la sélection directe sur une mémoire, sans échafaudage, plafonne à 0,51–0,54 (0 passant sur 4 graines) : [mesures et protocole](experiments/general-joint-negative/README.md).

**Le levier « regs ».** Quatre registres de lignée (RPUSH/RPEEK, `general` seul) ouvrent à la main un chemin neutre vers une clé p2 ou p3 (0,59 au mieux), mais l'évolution ne le recrute pas : 0 joint, le mur reste (0,863 ; 0,459) : [mesures](experiments/general-regs/README.md).

**Le démêlage à la naissance (`unmix_rate` 0,12).** Après le re-ciblage, la clé fait `LOAD 19 ; ADD` : elle ajoute l'indice courant (registre B) à la case retardée et mélange c et p2. Troisième réécriture à la naissance, même longueur : cet ADD devient l'intron `cf`, l'allèle de la lignée 3. Taux 0 hors general, donc aucun tirage dans les autres labos (hashes des 10 labos historiques inchangés). Sur la seed décalée et re-ciblée, ce geste seul donne mémoire-2 0,586 (code 3) : la table par indice courant, confirmée par hasard, passe encore devant. Les lignées doivent porter le reste. Dans le labo, 60 cycles, 200 ticks, `--no-reload`, génomes du pool qui passent `octet-adn general`. Avec le démêlage : graine 0 : 13, graine 1 : 13, graine 2 : 4, graine 3 : 13. Référence d649c29, même protocole : 11, 3, 0 et 6. **La graine 2 passe seule** : 4 génomes, tous avec `0x78 = 13` et `0x79 = cf`. Le meilleur (pool 5) fait mémoire-2 1,000 ; gelé 0,188 ; sans récompense 0,237 ; mémoire-3 0,347 ; known 0,729. Il fait 172 octets, à 12 octets de la seed, code 0. La mémoire-3 ne bouge pas : maximum 0,427 contre 0,435 dans la référence. Sur ce passant, aucune substitution d'un octet ne la porte au-dessus de 0,5. Les paires dans 0x77–0x95 vont jusqu'à 0,613 (code 3). Rien n'est écrit dans la seed. Mesure à part, hors labo, sur le même passant : un registre à deux cases réécrit à la main dans les 13 octets du bloc (20 → 19 → 18, clé `LOAD 18`, longueur 172) tient la mémoire-3 à 0,999, gelé 0,191, sans récompense 0,245. Cette ligne est écrite à la main : ce n'est pas une émergence. Elle montre que p3 tient à longueur fixe, à environ dix octets du passant. La case 18 qu'elle utilise est chaude sur la lignée 2 sauvée (un `LOAD 18` en fait un drapeau de début de tâche). Le second étage ci-dessous n'écrit pas dedans.

**Le second étage à la naissance (`deepen_rate` 0,12).** `deepen_prev` recopie la case 19 dans l'intron froid `0x10` avant qu'elle soit écrasée. `0x10` est hors des adresses 0 à 7 : ces cases reçoivent la valeur de l'indice, écrite comme une adresse par les `STORE` du génome. La clé lit `0x10`, donc l'indice d'il y a trois essais. Pour laisser cet indice courant dans le registre B, `MOV_BA ; LOADI 4 ; ADD` devient quatre `INC_A`, et les 13 octets du décalage deviennent la copie à deux étages. Longueur inchangée. Taux 0 hors general : aucun tirage, hashes des 10 labos historiques inchangés. Sans `SENSE ; AND 3` avant la clé, rien n'est écrit. Le passant sauvé de la graine 0 (`seed0-pool12`) a un `AND 0` à cet endroit : `deepen_prev` retourne 0 et l'ADN ne bouge pas. Sur le passant sauvé de la graine 2 (`seed2-pool3`), le geste seul donne mémoire-3 0,977, gelé 0,252, sans récompense 0,255. `gn_check` retourne 3 : la mémoire-2 retombe à 0,352. Ce fichier tient le critère ; ce n'est pas encore le labo.

**Course 60 cycles avec le second étage.** Même protocole que le démêlage : 200 ticks, `--no-reload`, quatre graines. `octet-adn general` sur le checkpoint dit émergence oui à chaque fois. Aucun génome ne tient les deux mémoires ensemble : si les deux passaient, le vérificateur annoncerait la mémoire-2 d'abord. Comptes du pool (indices 0 à 15 ; le champion, index 16, n'y est pas) : mémoire-3 / mémoire-2 = graine 0 : 10 / 3 ; graine 1 : 3 / 0 ; graine 2 : 10 / 0 ; graine 3 : 10 / 3. Meilleurs mémoire-3, tous à 172 octets : graine 0, 0,999 (36 octets de la seed, gelé 0,252 / 0,233, sans récompense 0,265 / 0,251) ; graine 1, 1,000, et le plus proche du critère est à 15 octets (mémoire-3 0,977, gelé 0,287 / 0,293, sans récompense 0,266 / 0,255) ; graine 2, 1,000 (37 octets, gelé 0,252 / 0,253, sans récompense 0,253 / 0,245) ; graine 3, 0,988 (28 octets, gelé 0,277 / 0,305, sans récompense 0,258 / 0,250). Notes affichées en fin de course : 0,556 (meilleure 0,578), 0,495 (0,497), 0,558 et 0,557 (0,560). Un génome de la graine 0 fait 173 octets : l'indel vient de la mutation, pas de l'opérateur. Le second étage échange la mémoire-2 contre la mémoire-3. Les graines 1 et 2 n'ont plus de passant mémoire-2 dans cette fenêtre (la graine 2 monte à 0,863, sous 0,95). Le champion affiché rate les quatre : mémoire-2 / mémoire-3 = 0,360 / 0,335, 0,369 / 0,409, 0,402 / 0,380 et 0,345 / 0,336. Un passant mémoire-3 de la graine 2 a le gelé à 0,000 / 0,000 : la chute dépasse 0,5, le critère formel passe, ce n'est pas le génome à citer. Rien n'est écrit dans la seed. Pas de nouvel opcode.

**Les deux mémoires, à la main, ne passent pas ensemble.** Sur le passant sauvé de la graine 2, après le second étage, un appendice (hors dépôt, aucun opcode, la seed intacte) laisse la clé sur la case 19 et ne la bascule sur `0x10` qu'une fois, quand une réponse déjà confirmée est fausse. Les sauts du moteur ajoutent l'immédiat non signé, modulo la longueur. Le génome fait 255 octets. Sans jamais basculer : mémoire-2 0,997, mémoire-3 0,404. Un compteur cumulé d'erreurs confirmées : une erreur donne 0,784 / 0,959 ; deux erreurs donnent 0,987 / 0,909. Toutes les fausses bascules de la mémoire-2 tombent avant l'essai 8 : la justesse reste 0,777, fenêtre coupée à 8, à 16 ou à 32, alors que la mémoire-3 continue d'arriver (0,698, puis 0,895, puis 0,944). Ignorer les premiers essais, puis basculer sur la suivante avant l'essai 64 : grâce 2 → 0,916 / 0,939 ; grâce 4 → 0,970 / 0,927 ; grâce 8 → 0,987 / 0,909 ; grâce 16 → 0,987 / 0,861. Repousser le plafond de 32 à 64, à grâce 4, ne change aucun chiffre. Gelé 0,260 / 0,252 et sans récompense 0,266 / 0,255 sur toute la frontière : la chute tiendrait. Aucun point n'a les deux justesses ≥ 0,95. Le plus près est la grâce 4, et `gn_check` annonce la mémoire-2 seule. Ce n'est pas le labo, et ce n'est pas un opérateur : publier cet appendice sélectionnerait un génome qui rate encore le critère joint. Grâce 4, puis bascule seulement à la deuxième contradiction confirmée (compteur à part, une seule bascule, cases 4 à 7 effacées, 255 octets) : mémoire-2 0,987, mémoire-3 0,901, code 2. Gelé 0,260 / 0,252, sans récompense 0,266 / 0,255. C'est plus loin du critère que la grâce 4 seule. Pas d'opérateur. Sonder les huit premiers essais, quatre sur la case 19 puis quatre sur `0x10`, et garder la clé la mieux payée (table 4 à 7 effacée, 286 octets) : mémoire-2 0,673, mémoire-3 0,646. La fenêtre est gratuite pour la note, pas pour le signal : le décalage ne s'y lit pas encore. Grâce 4, puis bascule seulement à la deuxième contradiction de la même case (263 octets) : mémoire-2 0,987, mémoire-3 0,654. Les contradictions qui feraient monter la mémoire-3 ne tombent pas deux fois sur la même case. Gelé 0,260 / 0,252, sans récompense 0,266 / 0,255, comme sur le reste de la frontière. Pas d'opérateur.

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
se divise qu'à ≥ 128 d'énergie. Après 60 cycles (graine 0, 200 ticks), le génome du pool le plus proche (65 octets) a changé le `JZ 1` du manque d'énergie en `JZ 99`. Remis à 1, sa note fraîche passe de 0,485 à 0,131 : le saut est sa stratégie, pas la cause des extinctions. Huit génomes sur seize portent cet immédiat. Les remettre tous à 1 fait passer le pool mélangé de 0,328 à 0,642. Le saut n'atterrit pas au même endroit : intron, `ATTACK` ou `SENSE`. Aucun génome n'a gardé `JZ 1`.

**pandemic.** À t = 20, 8 cas index. Nombres aléatoires communs : chaque tirage épidémique (contagion par paire d'agents et par tick, issue de la maladie, cas index) est un hachage indépendant des actions et du RNG du monde, et la fitness moyenne 4 paires appariées (même graine avec / sans effet des actions). Le pool garde les survivants les plus âgés (classer par énergie sélectionnait contre l'isolement, qui coûte). I → S voisin : p = 0.3 / tick ; E 3 ticks (sans symptômes) ; I
12 ticks puis mort (30 %) ou guérison. Stimulus propre à chaque créature (hook `before_vm`) :
bit0 malade, bit1 alerte reçue, bit2 immunisé. `EMIT` 1 = alerter (rayon 2), 2 = s'isoler ce tick,
3 = se vacciner (25 d'énergie). La baseline rejoue le même pool sur la même graine, actions sans effet.
Seed : s'isoler quand on est malade.

**code-repair.** Chaque créature reçoit un patient de 8 octets (SENSE_STIM ; MOV_BA ; 2 ops tirées
parmi INC/DEC/ADD/SUB/XOR/AND k ; EMIT ; YIELD), cassé par 1 ou 2 bit-flips. Le patient est mappé sur
`STORE`/`LOAD` imm 0xE0..0xE7 **uniquement si la créature a une fenêtre** (`win_len > 0`, posé par ce
seul labo). Signal : nombre de tests passés sur 16, + 0x40 si mieux que le meilleur vu. La réussite
exige l'équivalence sur les 256 entrées, pas seulement sur les 16 tests. Seed `repair` : recherche
locale bit à bit avec retour arrière (generate-and-validate). Protéger les 137 octets de cette échelle (aucun tirage) et ne transmettre que le génome de naissance empêche l'effondrement vu avant : sur 12 cycles les enfants restent près de 0,74 au lieu de tomber à 0,07, et l'échelle reste intacte. Sur 8 graines appariées, le pool (0,745) ne dépasse pas la seed (0,741). Les opcodes apparus dans la queue `0xFF` ne comptent pas : le même génome réduit à la même longueur, queue encore vide, a exactement les mêmes huit notes. `LOOP` en `0x88` n'y entre jamais. Le mécanisme n'est pas dans le dépôt.

**self-evolve.** Les octets 2..5 du génome sont ses paramètres d'évolution, lus à chaque SPLIT
(`mutate_evo`, uniquement quand `cfg.self_evo`) : substitution ≤ 12 %, indel ≤ 6 %, crossover on/off,
zone protégée ≤ 16 octets et ≤ la moitié du génome ; elle ne couvre jamais les octets 2..5 (dérive méta 8 %, pas ±4/±16, saturée) et coûte prot/4 d'énergie par SPLIT. Mortalité à 60 ticks (sinon « ne plus se reproduire » gèle tout). Cibles hachées, communes aux runs appariés. Fitness et baseline sont des moyennes sur 4 graines appariées. Une cible 0..15 change toutes les 50 ticks ; `EMIT v` rapporte 6 − 2·|v − R| si l'écart est < 4, sinon coûte 1. La cible n'est pas observable. La seed garde une mutation d'environ 4 % (octet 2 = 0x55), mange, se divise, puis `EMIT 5`. Geler les octets 2 et 3 à 0 fait perdre. Sur les 4 graines du test : fitness 0.210, écart +0.033. Les moyennes des paramètres dans la population finale sont dans `status.md` (`mut_rate`, `indel_rate`, `crossover`, `protected`).

**diagnose.** Le stimulus porte une marque `m = (m0 + tick) mod 8`. Elle change à chaque tick : la réciter avec `LOADI` ne tient pas. Le médicament noté est `((m + 1) XOR 4) mod 8`. La seed est un fourrageur court suivi d'un bourrage de `NOP` : elle n'a ni `SENSE_STIM` ni `EMIT`. On note les bits faux parmi les 3 bits bas : 0 → 1, 1 → 0,55, 2 → 0,20, 3 → 0,05. Copier `m` reste bas. L'incrément seul est toujours à un bit (0,55) : c'est le palier, pas le sommet. Une constante, qui ne suit pas la marque, tourne autour de 0,41. `((m + 1) XOR 4)` vaut aussi `(m − 3) mod 8` : relire l'historique (`SENSE_HIST`) suffisait. Ce labo vide l'historique à chaque tick. Le seul capteur utile est le stimulus courant. La baseline pose `m XOR 7` dans le stimulus et note toujours contre le vrai médicament. Le pool retient le génome au meilleur crédit par tick, mesuré sur au moins 40 ticks, seulement s'il s'est divisé : un calculateur stérile ne remplace pas la lignée. Premier palier, quand le médicament n'était que `(m + 1) mod 8`, graine 0, 150 cycles : un individu exact au cycle 19, population 0,78 au cycle 150 (écart +0,53), hold-out 0,75 contre 0,08, solo 1,000 / stimulus renversé 0,225, cœur `SENSE_STIM`, `INC_A`, `EMIT`. Sur le médicament à deux pas, le même cœur suivi de `MOV_BA`, `LOADI 4`, `XOR` est exact seul (1,000) et tombe à 0,375 si le stimulus est renversé. À 200 ticks et 5 % de mutation, le test mesure seed 0,400, copie 0,245, incrément 0,461, calcul complet 0,641 (écart +0,241). Course graine 0, 2000 cycles, depuis la seed aveugle : la lignée se divise encore (~240 naissances par cycle). La population passe 0,55 au cycle 105 puis reste autour de 0,57 (cent derniers cycles 0,576, écart +0,206 ; hold-out 8 graines 0,569). Un taux individuel de 1,000 apparaît (cycle 263) et n'est pas retenu : rejoué seul, ce génome vaut 0,377. Le calcul complet n'est pas devenu le pool.

**Qui se reproduit.** `commons`, `pandemic`, `antibiotic`, `self-evolve` et `diagnose` ne classent plus par l'énergie seule. `commons` baisse le score d'un génome qui mange une case sous 8 ou qui la vide. `antibiotic` crédite une cible tuée et débite les hôtes abîmés ; si le corps meurt, le pic positif de son génome reste en tête du pool. `pandemic` crédite l'isolement et l'alerte pendant la phase infectieuse, puis l'âge départage (l'énergie punissait ceux qui s'isolent). `self-evolve` crédite la justesse de l'`EMIT`.

**Exploits testés et fermés** (`test_new_labs_exploits_closed`) : ne rien faire (fold, commons,
code-repair → 0 ; pandemic → ≈ 0 malgré zéro mort, car la population n'est pas maintenue) ; tuer les
hôtes pour affamer le pathogène (→ 0, facteur hôtes vivants) ; arroser des 8 médicaments (dégâts) ;
vacciner ou s'isoler en permanence (coût) ; balayer 0..15 dans self-evolve ; geler les octets 2 et 3
(la seed porte déjà un taux de mutation, le gel fait perdre) ; surapprendre les 16 tests de code-repair (refusé par
l'équivalence sur 256 entrées) ; sélectionner des pathogènes faibles (ils ne sont pas dans le pool).

**Sélection entre cycles (appariée, robuste au bruit)** : après le cycle, le nouveau pool
(les survivants les mieux classés) et le pool en place sont **réévalués sur les mêmes
`SEL_EVALS` = 4 graines** (indépendantes de celle du cycle). Le nouveau pool remplace l'ancien
s'il fait au moins aussi bien en moyenne ; sinon l'ancien pool est retenté avec un autre aléa.
L'ancienne règle (« garder si ≥ 90 % du meilleur historique », qui gelait un labo derrière un
pic chanceux) reste disponible via `--select legacy`. Coût : ~9 cycles simulés par labo et
par cycle (≈ 0,31 s CPU pour les 10 labos historiques, ≈ 1,3 s pour les 16 ; commons tourne sur 5 × ticks,
antibiotic, commons, code-repair rejouent une baseline, pandemic et self-evolve 4 paires appariées). Les labos de communication / prédiction donnent aussi de l'énergie aux
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
- Labos humanité (40 cycles, graines 0,1,2, eval 8 graines tenues à l'écart, mesures d'avant le classement par contribution) : `fold`, `antibiotic`, `commons`, `code-repair` et `pandemic` battent leur baseline. Les pools évolués de `pandemic` battaient la seed (0.60 à 0.76 contre 0.44). **`code-repair`** : sur 12 cycles le pool était encore la seed, écart aux flips +0.329 sur 8 graines tenues à l'écart ; sur 24 cycles, 0 acceptation. `org-seeds` importe le génome du pool, pas le sommet d'un cycle refusé. **`self-evolve`** : mètre actuel (4 paires, régimes de 50 ticks, mort à 60). La seed à mutation ≈ 4 % fait +0.033 sur les 4 graines du test. La même seed gelée à mut = indel = 0 tombe à 0.147 (−0.063). Un gel par STORE à chaque tick faisait −0.010 : il ne bat pas le moteur fixe. Dérive méta 3 % ±1/±3 → 8 % ±4/±16 (pools de 40 cycles, graines 0..15, eval 8 graines tenues à l'écart) : écart moyen aux paramètres fixes −0.024 → −0.003, mut descend vers 0.025-0.029 ; toujours pas au-dessus du moteur fixe.

## Trois éprouvettes et corrections — ordre 0996a2cd

Protocoles committés avant les mesures, en C séparé du runner et sans opcode ajouté :

| approche | résultat à son seuil pré-enregistré | dossier |
|---|---|---|
| perception par rejeu de la séquence exacte des actions | non : 4/32 graines, seuil 24 | [perception-sequence](experiments/perception-sequence/README.md) |
| sélection lexicase par épisodes d'apprentissage | non : 0/32 mémoires jointes, seuil 8 et avance de 6 sur la moyenne | [selection-episodes](experiments/selection-episodes/README.md) |
| recyclage de métabolites entre deux classes héréditaires | oui : 10/32 communautés reproductrices dépendantes, seuil 8 | [symbiose-metabolique](experiments/symbiose-metabolique/README.md) |

Le oui métabolique porte sur une chimie et deux classes offertes, sans preuve
que la mutation ait inventé une nouvelle capacité ou la coopération.
[Corrections mesurées](experiments/corrections-0996a2cd/README.md) : tick témoin
isolé pour les rendez-vous, refus de nurserie exercés au plafond 4096,
protection des 14 noms de mondes ; sonde-phase v2 existante revalidée.

Construire chaque ELF dans un chemin **jamais utilisé**, sans `make` nu ni
`make test` sur un hôte où des mondes vivent. Les cibles
`experiments/approches-build/IDENTIFIANT_NEUF/{perception-sequence,selection-episodes,symbiose-metabolique,corrections,historiques,sonde-phase}`
compilent directement les sources et refusent d'écraser un ELF. Aucun `.so`,
état de monde ou fichier de run n'est nécessaire pour ces mesures.

## Générateur de mondes successifs — Passage (27/10/2026)

Pour les prochaines fondations compatibles, utiliser désormais [`experiments/generateur-20261027/generator.py`](experiments/generateur-20261027/README.md): paramètres aléatoires reproductibles, transplantation d'états natifs existants, chimie des256valeurs d'octet, mises à jour et injections en vie. La cohorte initiale contient420descendants de32mondes, sans transcodage des génomes ou mémoires. Le premier monde `vie-passage-001-20261027` vit dans `octet-passage-001`, avec son broker Grokipedia séparé. Relief et Transmission précédents restent vivants et distincts; aucun ancien monde arrêté n'est relancé. Les protocoles quantiques et certaines tâches de laboratoire restent des contextes catalogués, pas des lois prétendues exécutées. Voir le README du générateur pour les limites et la sélection d'une cohorte suivante.
