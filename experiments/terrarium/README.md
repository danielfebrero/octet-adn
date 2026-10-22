# Terrarium : un monde ouvert qu'on laisse vivre

Pas de labo, pas de fitness. Une écologie libre (nourriture qui repousse, énergie, `SPLIT` normal, mutation et
duplication du moteur) qu'on laisse tourner aussi longtemps qu'on veut, avec des checkpoints et une reprise,
pendant qu'on enregistre des séries temporelles. On cherche à savoir si ça continue d'inventer
(open-ended) ou si ça plafonne.

Le cœur n'est pas modifié (`src/`, `include/`, `seeds/` intacts) : tout passe par `world_step` et le hook
`before_vm`. Les 10 labos historiques gardent le même hash.

## Monde

- **Archipel** : un `World` est plafonné à 64×32 et `CMAX` = 1024 créatures. Le terrarium monte donc K îles
  (4 par défaut, un thread par île), reliées en anneau. Toutes les `--migrate` ticks, ~1 % de chaque île part
  vers la suivante, à une position aléatoire. Taille configurable : `--islands --w --h --max-pop --regrow`.
- **Fondateurs** : 64 par île, pris en rotation dans les 16 seeds du dépôt (predator = espèce 1).
  Avec `--founders random`, ce sont des génomes aléatoires de 32 à 256 octets.
- **Stimulus** : le quartet haut est un environnement de Markov à 16 états, qui change avec p = 1/50 par tick.
  Le quartet bas est le **canal social** : la valeur `EMIT & 15` d'un voisin 3×3 qui a émis au tick précédent,
  tirée au hasard, en excluant soi-même. S'il n'y a aucun émetteur voisin, le quartet bas vaut l'environnement.
  `--channel off` ne livre jamais l'émission (témoin).
- **Saisons** (option) : `--season-amp a --season-period p` module la repousse par 1 + a·sin(2πt/p).
  Cela sert de témoin positif pour le détecteur de cycles.

## Mesures (`series.tsv`, une ligne toutes les `--sample` ticks)

| colonne | sens |
|---|---|
| `pop`, `births`, `deaths` | population totale, naissances et morts dans la fenêtre |
| `n_geno`, `H_geno`, `n_raw` | génotypes distincts et leur entropie (bits). Hash « fonctionnel » : opcodes valides non-NOP + immédiat, à leur position. `n_raw` = hash brut du germen |
| `persist_cum`, `new_persist`, `persist_alive` | **Bedau-Packard, niveau génotype** : génotypes nouveaux restés présents ≥ 10 échantillons consécutifs avec ≥ 3 porteurs simultanés (les fondateurs ne comptent pas) |
| `shadow_cum` | même compteur pour une **ombre neutre** : étiquettes qui naissent et meurent avec les mêmes nombres et le même taux d'innovation que le réel, mais au hasard (sans sélection) |
| `n_comp`, `comp_per_ind`, `comp_cum`, `comp_new`, `comp_alive` | **Bedau-Packard, niveau composant** : un composant est un 5-gramme d'opcodes valides consécutifs (sans position ni immédiats), par espèce. Il est compté quand il est porté par ≥ max(3, 1 % de la pop) individus pendant 10 échantillons consécutifs |
| `len_mean`, `len_max`, `coding_mean` | longueur des génomes et nombre d'octets codants |
| `niches_occ`, `stable_niches`, `H_behav` | **spécialisation** : profil comportemental de chaque créature sur la fenêtre (fractions de ticks où elle bouge, mange, tourne, se divise, émet), chaque fraction binée en < 5 % / < 50 % / ≥ 50 %, soit 243 niches. Une niche est stable si elle contient ≥ 5 % de la pop pendant 10 échantillons |
| `soc_pairs`, `soc_mi`, `soc_null`, `soc_excess` | **MI sociale**, en bits par paire, détaillée ci-dessous |
| `emits` | nombre d'EMIT dans la fenêtre |

**MI sociale.** On définit l'action d'une créature à un tick comme (EMIT & 15 ou rien) × (a bougé, a tourné,
s'est divisée), soit 136 symboles. On exclut volontairement « a mangé » : c'est un résultat mécanique (EAT réussit
ssi F > 0), la fuite trouvée dans mi-detector. Un codeur bayésien séquentiel (Dirichlet hiérarchique) donne :

- `soc_mi` = L(a | a₋₁) − L(a | a₋₁, sig), où sig est l'émission *livrée* ;
- `soc_null` = la même chose avec nsig, l'émission d'un *autre* voisin émetteur au même tick, qui n'a pas été
  livrée. Elle a la même localité et le même environnement, mais aucun lien causal.

`soc_excess` = `soc_mi` − `soc_null`. Il n'est > 0 que si recevoir le signal change l'action. On ne code que les
créatures qui ont ≥ 2 voisins émetteurs, avec un plafond de 20 000 paires par île et par fenêtre.

**Cycles.** Autocorrélation, après retrait de la tendance, des 96 derniers échantillons de `pop` et de chaque
niche stable. Un cycle est signalé quand l'ACF passe par un creux < −0,2 puis par un pic > 0,4 à un lag ≥ 4.
Le détecteur est réarmé après 32 échantillons calmes.

**Événements** (`events.tsv` : tick, type, détail) :

- `social` : excès > `--soc-thr` (0,05 bit/paire), avec ≥ 1000 paires, sur 3 fenêtres d'affilée. Le seuil
  vaut environ 2,5× le max observé sur les témoins canal-off (0,048).
- `niche_stable` et `niche_perdue` (perdue = < 2 % pendant 10 échantillons).
- `cycle` (série, période en ticks, ACF).
- `extinction`.

## Lancer, reprendre, faire tourner en continu

```sh
make experiments/terrarium_run
./experiments/terrarium_run run --dir runs/terra1 --ticks 200000        # jusqu'au tick 200 000 (absolu)
./experiments/terrarium_run run --dir runs/terra1 --ticks 400000        # reprend depuis runs/terra1/state.ck
./experiments/terrarium_run report runs/terra1                          # résumé par quarts + événements
```

**En continu** : `--ticks 0` tourne sans fin. Un checkpoint est écrit toutes les 2000 ticks (`--ck-every`),
de façon atomique (tmp + rename). SIGINT ou SIGTERM arrêtent proprement, avec un checkpoint. Relancer la même
commande reprend où on s'était arrêté : les paramètres viennent du checkpoint et les TSV sont tronqués au tick
du checkpoint.

```sh
nohup ./experiments/terrarium_run run --dir runs/terra-long --ticks 0 --islands 8 > runs/terra-long.log 2>&1 &
tail -f runs/terra-long/events.tsv
```

Ordres de grandeur, sur 8 cœurs :

- 4 îles, ~3000 créatures : ~1400 ticks/s seul, ~470 ticks/s avec 8 runs en parallèle. 1 M ticks ≈ 12 min seul.
- Disque : ~140 Ko de `series.tsv` pour 1000 échantillons, checkpoint ~14 Mo (4 îles, ~3000 créatures).
- `--islands 8` occupe 8 cœurs.

**Reproductibilité** : déterministe pour une graine donnée (une Rng par île, des segments synchronisés aux
frontières d'échantillon et de migration). Un run interrompu puis repris donne le même hash et le même
`series.tsv` qu'un run d'une traite. `selftest` le vérifie, et c'est appelé par `make test`.

## Démo (5 min 26 s au total sur 8 cœurs, chiffres dans `results.txt`)

```sh
make experiments/terrarium_run && B=$PWD/experiments/terrarium_run && mkdir -p /tmp/terra-demo && cd /tmp/terra-demo
( $B run --dir real1 --seed 1 --ticks 100000 && $B run --dir real1 --ticks 200000 ) 2>real1.log &   # coupé puis repris
$B run --dir real2    --seed 2 --ticks 200000                   2>real2.log &
$B run --dir real3    --seed 3 --ticks 200000                   2>real3.log &
$B run --dir mut0     --seed 1 --mut 0 --dup 0 --ticks 200000   2>mut0.log &
$B run --dir chanoff1 --seed 1 --channel off --ticks 200000     2>chanoff1.log &
$B run --dir chanoff2 --seed 2 --channel off --ticks 200000     2>chanoff2.log &
$B run --dir random1  --seed 1 --founders random --ticks 200000 2>random1.log &
$B run --dir season1  --seed 1 --season-amp 0.8 --ticks 200000  2>season1.log &
wait; $B report real1 real2 real3 chanoff1 chanoff2 mut0 random1 season1
```

8 runs de 200 000 ticks (1000 échantillons) :

- **real1–3** : trois graines, canal on. real1 a été coupé à 100k puis repris.
- **chanoff1–2** : canal off, témoin social.
- **mut0** : mutation et duplication à 0, témoin de nouveauté.
- **random1** : fondateurs aléatoires.
- **season1** : saisons de période 4000, témoin positif des cycles.

| | real1 | real2 | real3 | chanoff1 | chanoff2 | mut0 | random1 | season1 |
|---|---|---|---|---|---|---|---|---|
| pop finale | 3037 | 2921 | 2987 | 2764 | 2914 | 1345 | 431 | 1399 |
| H_geno (bits) | 11.57 | 11.51 | 11.54 | 11.43 | 11.51 | 1.00 | 8.58 | 10.45 |
| persist_cum (ombre) | 4 (0) | 59 (10) | 9 (0) | 3 (1) | 0 (0) | 0 (1) | 962 (0) | 3 (0) |
| comp_cum | 142 825 | 36 822 | 129 772 | 77 953 | 131 842 | 0 | 46 078 | 735 349 |
| comp_new / 10k ticks, Q1 → Q4 | 5573 → 9200 | 4436 → 804 | 6021 → 7881 | 6611 → 367 | 7532 → 3951 | 0 | 3378 → 1874 | 28 537 → 31 387 |
| len_mean, Q1 → Q4 | 335 → 413 | 275 → 270 | 299 → 386 | 326 → 325 | 330 → 390 | 256 | 138 → 121 | 336 → 407 |
| niches stables (fin) | 4 | 5 | 4 | 4 | 3 | 7 | 2 | 3 |
| excès social poolé (bits/paire) | +0.0050 | +0.0009 | +0.0011 | −0.0001 | −0.0003 | +0.106 | +0.112 | +0.0055 |
| excès social Q4 | −0.0000 | +0.0002 | +0.0001 | −0.0015 | +0.0012 | — | −0.0034 | +0.0204 |
| événements social / cycle | 1 / 0 | 0 / 0 | 0 / 0 | 0 / 2 | 0 / 4 | 0 / 0 | 1 / 0 | 1 / 16 |

**Lecture**

- **Diversité** : élevée et stable, mais elle ne croît pas. On a à peu près un génotype par individu
  (H ≈ log2(pop)), parce qu'avec 2 % de mutation par octet sur ~300 octets un clone est rare (~e⁻⁶).
  À ce niveau, Bedau-Packard est donc presque vide : 4 / 59 / 9 génotypes persistants, contre 0 / 10 / 0 pour
  l'ombre.
- **Composants** : ici la nouveauté ne s'arrête pas sur 200k ticks dans real1 et real3. Elle augmente même
  rapportée aux naissances × octets codants (0.72 → 1.98 et 1.59 → 2.26 ×1e-4). Mais :
  - elle suit la croissance des génomes : real1 passe de 335 à 413 octets, real3 de 299 à 386 ;
  - dans real2, où le génome reste à ~270 octets, elle décroît de 4.4k à 0.8k ;
  - les témoins canal-off font la même chose ;
  - faute d'ombre au niveau composant, on ne peut pas la distinguer d'une dérive. mut0 donne bien 0, ce qui
    montre que la mesure ne compte pas les fondateurs.
- **Social** : le canal porte un signal réel au début seulement (t ≤ 6800, fondateurs echo et relais), avec un
  événement à t = 6800 dans real1. Il disparaît ensuite : en Q4 l'excès vaut 0.0000 / 0.0002 / 0.0001 bit/paire,
  dans le bruit des témoins (±0.0015). Témoin positif : mut0, où les fondateurs restent figés, donne +0.106
  tant qu'il y a des paires.
- **Spécialisation** : 3 à 5 niches comportementales coexistent de façon stable, installées avant t ≈ 17k.
  Il y a deux remplacements tardifs (real1 à 155k, real2 à 168k), mais pas d'accumulation.
- **Cycles** : aucun dans les runs réels. Les témoins canal-off montrent des cycles de 1200 à 1800 ticks à
  ACF 0.40–0.42, juste au seuil, donc faibles. Le témoin saison est bien retrouvé : 16 cycles de 3200 à
  4200 ticks pour une période imposée de 4000.

**Verdict : plateau.** On obtient un écosystème stable, peuplé, divers et spécialisé (3 à 5 niches), qui ne
s'éteint pas et qu'on peut faire tourner indéfiniment. Mais on ne voit aucun signe d'ouverture à 200k ticks :

- la nouveauté de composants suit la longueur des génomes et existe aussi sans canal social ;
- l'usage social du canal, présent chez les fondateurs, est perdu par l'évolution ;
- aucune nouvelle niche n'apparaît après la phase initiale ;
- il n'y a pas de cycle endogène net.

C'est un résultat négatif honnête. La plateforme mesure, et ses témoins positifs (saison, mut0 social, selftest
du codeur) répondent comme attendu.

## Limites et pistes

- **Pas d'ombre au niveau composant.** Une vraie ombre neutre sur les composants (Bedau 1998) demanderait
  de répliquer la généalogie. C'est la prochaine mesure à ajouter pour trancher entre dérive et sélection.
- **mut0 n'est pas une absence totale de nouveauté.** `SPLIT` copie le soma, donc des éditions par `STORE`
  sont héritées. Ici elles sont restées marginales : 2 génotypes en tout.
- **Le canal ne paie rien.** Émettre ne rapporte rien de façon intrinsèque : rien ne sélectionne la
  communication. Une pression (prédation, nourriture partagée, signal d'alarme) est la piste naturelle pour le
  social.
- **Taille.** On peut monter jusqu'à 16 îles ; au-delà, il faut des îles plus nombreuses, pas plus grandes
  (le plafond `CMAX` est dans le cœur).
