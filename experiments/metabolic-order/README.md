# metabolic-order : un paramètre d'ordre métabolique (capter plus qu'un marcheur aveugle, et tenir)

Idée 5 des « 5 idées out of the box ». C'est un prototype, **sans PR**.

## Hypothèse
Un vivant capte et stocke de l'énergie **mieux qu'un marcheur aléatoire de même motricité**,
et il tient cet écart dans le temps. Le bruit et un ELF devraient scorer bas.

## Méthode (aveugle au code : on ne lit que des observables)
- **Run génome** : 4 copies dans un monde `cfg_default` (48×20, sans labo), 800 ticks, 6 répliques.
  On mesure la biomasse `B_g(t) = Σ énergie des vivants`. Par créature-tick, on mesure aussi la motricité :
  `λ_step` (pas/tick), `p_turn` (changements de direction ×4/3) et `p_split` (divisions / ticks où e ≥ seuil).
- **Run nul**, avec la même graine monde (nombres aléatoires communs) : le génome est réduit à `YIELD`.
  Chaque tick, le hook fait tourner le marcheur au hasard (`p_turn`), lui fait faire `λ_step` pas,
  le fait manger, et le divise avec `p_split` si e ≥ seuil. Il a la même motricité et le même coût,
  mais aucune information sur le monde.
  - `M` : marcheur matché (1 bouchée par tick).
  - `M_glouton` : marcheur qui mange après chaque pas puis vide sa case. C'est le témoin dur.
- `M = moyenne_{t∈(T/2,T]} (B_g − B_w) / (4·E0)`.
- `tenue` = fraction des ticks de la 2ᵉ moitié où `B_g > B_w`.
- Le cœur n'est pas touché : on passe par le hook `before_vm` et le scratch `ls[]`, hors labo.
  Le hash des 10 labos historiques reste donc identique.
- `selftest` est branché dans `make test` : il vérifie le déterminisme et que le génome inerte donne `M = 0`.
- Gene-calling : chaque octet est remplacé par `0xFF`, et on mesure ΔM.

## Contrôles
Ce sont les mêmes que pour feat/mi-detector (`experiments/lifeprobe.h`, `experiments/controls/evolved/`) :

| groupe | contenu |
|---|---|
| seeds | les 16 seeds |
| évolués | 212 génomes issus des 18 labos |
| bruit | 200 génomes de bruit |
| ELF | 200 fenêtres de `/bin/ls` (sha256 `833d6f9c…808a5c`) |
| évolués-mélangés | 200 génomes évolués dont les octets sont permutés |

## Résultats (`results.txt`, `./experiments/metabolic_order controls`, ~65 s)
| groupe | médiane M | p90 | > p95 du bruit | médiane M_glouton |
|---|---|---|---|---|
| seeds | 0.146 | 0.376 | 56 % | 0.151 |
| évolués | 0.024 | 0.870 | 50 % | 0.053 |
| bruit | 0.000 | 0.000 | 5 % | 0.000 |
| ELF | 0.000 | 0.000 | 6 % | 0.000 |
| évolués-mélangés | 0.000 | 0.170 | 22.5 % | 0.000 |

AUC des évolués contre chaque groupe, selon l'observable :

| observable | vs bruit | vs ELF | vs mélangés |
|---|---|---|---|
| M | 0.671 | 0.624 | 0.630 |
| M_glouton | 0.723 | 0.682 | 0.672 |
| tenue | 0.906 | 0.932 | 0.781 |
| **survie seule** | **0.889** | **0.925** | 0.757 |

**À survie égale** (lignées vivantes à T dans toutes les répliques), l'AUC de M ne vaut plus que
0.605 contre le bruit et **0.302 contre l'ELF**. Les 6 fenêtres ELF qui survivent battent leur
marcheur (médiane +0.34) plus nettement que les évolués (+0.10).

Gene-calling :
- `seed:forager` : 11 octets sur 256 pèsent plus de 0.05 (TURN, JMP, STORE, STEP, LOAD, JZ, EAT…).
- `predict-walk` évolué : 14 octets sur 256.
- Dans les deux cas, ce sont les **mêmes octets 0x002–0x01d que le gene-calling de feat/mi-detector**.
  Deux observables indépendants pointent donc le même noyau d'une vingtaine d'octets.
- `diagnose` évolué : M = 0, aucun gène détecté. Il ne survit pas seul.

## Verdict honnête : négatif pour le paramètre d'ordre
- Le bruit et l'ELF scorent bas (médiane 0, p90 0), comme demandé. Mais c'est surtout parce
  qu'**ils meurent**. M sépare **moins bien que la survie seule**, et la tenue n'est guère plus
  qu'un indicateur de survie.
- Le marcheur matché est un **témoin insuffisant**. Une fenêtre d'ELF lente (`λ≈0.3`) fait mieux
  que son marcheur. Plusieurs évolués et mélangés en ligne droite (`λ=1, p_turn=0`) gagnent environ
  +0.87 E0 parce que leur marcheur meurt, sans que l'écart vienne d'une perception. La régularité
  temporelle des actions, la coordination des divisions, etc. échappent au matching par taux moyens.
- Dans ce monde, la biomasse est limitée par la repousse (4 cases par tick) et par `max_pop`. Les
  bons fourrageurs saturent, et l'écart au hasard reste petit (seeds +0.15 à +0.28 E0).

## Suite
- Matcher le marcheur sur les **séquences** d'actions plutôt que sur les taux : rejouer la trace
  d'actions du génome, mais décalée ou permutée par blocs. Cela isole l'apport de la perception.
- Tester dans un monde pauvre ou fluctuant, où un vivant qui stocke doit battre le hasard.
- Combiner avec le score de feat/mi-detector.

## Usage
```
make experiments/metabolic_order
./experiments/metabolic_order controls [dossier_ck] [elf]
./experiments/metabolic_order mo seed:forager fichier.ck
./experiments/metabolic_order genes seed:forager
./experiments/metabolic_order selftest
```
