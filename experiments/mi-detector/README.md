# mi-detector — détecter la vie par compression (information mutuelle percept → action)

Idée 2 des « 5 idées out of the box ». Prototype, **pas de PR**.

## Hypothèse
Un vivant modélise son monde : ses actions portent de l'information sur ce qu'il perçoit
(*réactivité*) et, s'il a une mémoire, sur ce qu'il a perçu **avant** (*composante prédictive*).
Un octet mort ou du bruit agit indépendamment du monde.

## Méthode (aveugle au code : on ne lit QUE la trace)
- Un génome est posé seul dans un monde `cfg_default` (48×20, sans labo), 400 ticks, 8 répliques
  (graines monde `0xA11CE000+k`). On suit la lignée : le fondateur, puis à sa mort le plus vieux
  vivant (tous sont ses descendants).
- Observation via le hook `before_vm` + deltas d'état (`steps`, `eaten`, `splits`, `d`, `emitted`).
  **Le cœur (vm.c / world.c) n'est pas touché** → hash des 10 labos historiques identique.
- Par tick : action `A_t` = (EMIT&15 ou rien) × {a bougé, mangé, tourné, divisé} (272 symboles) ;
  percepts `S_t` = stimulus (processus de Markov, change avec p=1/4), `F_t` = nourriture devant (3 seaux),
  passé `P1,P2` = deux dernières valeurs de stimulus ≠ `S_t`.
- Codeur séquentiel adaptatif (Dirichlet hiérarchique, β=0.5) : longueurs de code en bits, coût
  d'apprentissage payé, pas de sur-apprentissage.
  - `I_react = (L(A|A₋₁A₋₂) − L(A|A₋₁A₋₂,S,F)) / T`
  - `I_mem   = (L(A|…,S,F) − L(A|…,S,F,P1,P2)) / T`
  - **témoin nul** : même calcul avec les percepts d'une AUTRE réplique (indépendants des actions).
    Les contextes en plus coûtent toujours un peu (fragmentation), le nul est donc négatif ;
    le score retenu est **MI = (I_react+I_mem) − nul** : « combien les vrais percepts compriment mieux
    que des faux ». Pour un génome aveugle, MI ≈ 0 en espérance ; pour le génome inerte, MI = 0 exactement
    (vérifié par `selftest`, branché dans `make test`).
  - `q4` : même score sur le dernier quart de l'horizon seul (persistance).
- Gene-calling : chaque octet remplacé par `0xFF` (intron sûr), `ΔMI` mesuré.

## Contrôles (`experiments/lifeprobe.h`, partagé avec feat/metabolic-order)
| groupe | contenu |
|---|---|
| seeds | les 16 seeds du dépôt |
| évolués | 212 entrées de pool des 18 `latest.ck` locaux copiés dans `experiments/controls/evolved/` |
| bruit | 200 génomes uniformes, longueurs tirées des évolués |
| ELF | 200 fenêtres de `/bin/ls` (158632 o, sha256 `833d6f9c…808a5c`), #0 = en-tête |
| évolués-mélangés | 200 évolués dont les octets sont permutés (même composition, ordre détruit) |

## Résultats bruts du score MI d'origine (`results.txt`, `./experiments/mi_detector controls`, ~14 s) — voir la section fuite
| groupe | n | médiane MI | p90 | > p95 du bruit |
|---|---|---|---|---|
| seeds | 16 | 0.157 | 0.603 | 68.8 % |
| évolués | 212 | 0.174 | 0.595 | 58.5 % |
| bruit | 200 | 0.001 | 0.090 | 5.0 % |
| ELF | 200 | 0.000 | 0.030 | 2.5 % |
| évolués-mélangés | 200 | 0.008 | 0.105 | 7.0 % |

AUC évolués > bruit **0.880**, > ELF **0.935**, > mélangés **0.844**, > seeds 0.495.
À vie égale (lignées vivant ≥ 95 % de l'horizon) : AUC bruit 0.783, ELF 0.852, mélangés 0.810 →
le score n'est **pas** qu'un proxy de survie. Persistance q4 : AUC bruit 0.856, ELF 0.887.

Gene-calling : sur `seed:echo` 19/256 octets comptent (> 0.01 bit/tick), sur `predict-walk` évolué 18/256,
tous dans les ~32 premiers octets (le reste est du remplissage neutre) ; `diagnose` évolué : 7/33 dont
`SENSE_STIM`, `EMIT`, `SENSE_NB`.

## Verdict honnête
- **Positif (à relativiser, cf. fuite)** : le détecteur sépare nettement le vivant (seeds + évolués) du bruit et de l'ELF
  (≈ 60 % des évolués au-dessus du p95 du bruit, contre 2.5 % pour l'ELF), et survit au contrôle
  de composition (mélangés) et au contrôle de survie.
- **Négatif** : la composante **mémoire / prédictive est nulle ou négative partout** (médiane évoluée
  −0.065, AUC 0.27 vs bruit) : aucun génome, seed ou évolué, n'utilise de façon détectable le
  stimulus passé au-delà du présent. Ce qui est détecté, c'est de la **réactivité**, pas de la prédiction.
- ~40 % des évolués scorent bas : lignées qui meurent seules (commons, code-repair…) ou dont la niche
  est sociale/labo — un monde vide n'excite pas leur comportement. Faux positifs max : un mélangé de
  predator-prey (0.34), un bruit (0.21).
- ~~Fuite possible~~ → mesurée, voir ci-dessous : **la majeure partie du score MI vient d'une fuite
  d'issue**, pas de la perception.

## Fuite : ce que c'est, et combien elle pèse
« Fuite » = de l'information sur le percept qui apparaît dans les actions **sans que l'agent l'ait perçue**.
Le témoin nul (percepts d'une autre réplique) ne retire que la part due au hasard ; il ne retire pas les
liens causaux monde ← agent. Deux mécanismes, deux mesures :

1. **Fuite motrice (boucle fermée)** : `F_t` (nourriture devant) dépend des pas/repas passés de l'agent.
   Mesure : **boucle ouverte** — la trajectoire enregistrée (position, cap, repas) est rejouée par une
   marionnette aveugle (génome `YIELD`, téléportée chaque tick) dans le monde d'une autre réplique ;
   le `F_ouvert` qu'elle rencontre est causé par la trajectoire mais n'a pas pu la causer.
   `fuite motrice = I_F(ouvert) − I_F(nul)`.
2. **Fuite d'issue** (trouvée grâce aux témoins aveugles) : le bit « a mangé » de `A_t` est le
   **résultat physique** d'EAT, qui réussit ssi `F_t > 0`. Ce n'est pas un choix. Mesure : tout recoder sur
   `A⁻` = A sans ce bit. `fuite d'issue = [I_F(réel)−I_F(nul)] − [I_F⁻(réel)−I_F⁻(nul)]`.
3. **MI_S** : n'utilise que le stimulus exogène `S_t` (+ `P1,P2`), tiré par un RNG séparé que l'agent
   ne touche jamais → **aucune fuite possible** par construction, même témoin nul.

| mesure (évolués, n=212) | médiane | p90 | AUC vs bruit | ELF | mélangés |
|---|---|---|---|---|---|
| MI (ancien score) | 0.174 | 0.595 | 0.880 | 0.935 | 0.844 |
| fuite motrice `I_F(ouv)−I_F(nul)` | −0.007 | 0.077 | 0.33 | 0.34 | 0.34 |
| fuite d'issue (bit « a mangé ») | **0.133** | 0.305 | 0.859 | 0.884 | 0.812 |
| `I_F(réel)−I_F(nul)` (part attribuée à F) | 0.180 | 0.334 | 0.913 | 0.959 | 0.855 |
| perception de F sur A⁻, sans les deux fuites | 0.006 | 0.109 | 0.668 | 0.711 | 0.617 |
| **MI⁻** (S,F,P sur A⁻) | 0.039 | 0.551 | **0.702** | **0.780** | **0.661** |
| **MI_S** (S seul, zéro fuite) | 0.018 | 0.347 | **0.646** | **0.719** | **0.620** |
| MI_S à vie égale (≥ 95 %) | 0.018 | 0.347 | 0.615 | 0.887 (n=3) | 0.586 |

Témoins aveugles (aucun capteur) :
| génome | MI | MI_S | MI⁻ | `I_F` réel−nul | fuite motrice |
|---|---|---|---|---|---|
| STEP-EAT | **0.334** | 0.003 | 0.000 | 0.359 | −0.005 |
| EAT-x3-STEP | **0.297** | 0.020 | 0.000 | 0.317 | −0.015 |
| EAT-SPLIT-STEP | 0.049 | 0.012 | 0.021 | 0.016 | −0.021 |
| STEP-EAT-TURN | 0.024 | −0.003 | 0.000 | 0.029 | −0.002 |

Un génome de 2 octets sans aucun capteur scorait 0.33 bit/tick, plus que la médiane des évolués.
Sur A⁻ il retombe à 0 exactement.

**Verdict sur la fuite.**
- La fuite motrice de boucle fermée, celle qu'on soupçonnait, est **petite et non discriminante**
  (médiane ≈ 0, p90 0.08 bit/tick, AUC ≈ 0.33).
- La vraie fuite, c'est l'issue du repas : environ **0.13 bit/tick, soit ~75 % de la médiane du MI**.
  Elle explique l'essentiel de l'AUC de 0.88. Elle sépare quand même le vivant du bruit (manger souvent
  est un trait des survivants), mais elle mesure « mange quand il y a à manger, par construction »,
  pas « perçoit ».
- **Sur S_t seul, le détecteur sépare encore, mais faiblement** : AUC MI_S 0.65 vs bruit, 0.72 vs ELF,
  0.62 vs mélangés. Seulement 24 % des évolués (37.5 % des seeds) dépassent le p95 du bruit, contre
  59 % avec l'ancien MI.
- La version propre combinée, MI⁻, donne une AUC de 0.70, 0.78 et 0.66.
- Les seeds réactifs au stimulus restent nets (echo MI_S 0.33, walk 0.39, neural 0.13).
- forager est purement réactif à F : MI_S −0.02, perception de F réel−ouvert ≈ 0.53 sur A.
- Faux positifs MI_S max : un mélangé de diagnose (0.26), un bruit (0.19), une fenêtre ELF (0.16).
- Conclusion : le détecteur « vivant vs mort » à 0.88/0.94 était **largement gonflé**. Le signal
  perceptif honnête vaut une AUC de 0.62 à 0.78. Il est réel mais modeste, et porté par une minorité
  de génomes qui lisent vraiment le stimulus.

## Suite
Labo « prédiction » où le stimulus passé détermine la nourriture future (pression sur `I_mem`),
puis réutiliser `I_mem` comme fitness ; tester le relais sur un monde peuplé (signaux d'autrui).

## Usage
```
make experiments/mi_detector
./experiments/mi_detector controls [dossier_ck] [elf]
./experiments/mi_detector mi seed:echo fichier.ck    # affiche aussi MI_S, I_F réel/ouvert/nul, MI⁻
./experiments/mi_detector genes seed:echo
./experiments/mi_detector selftest
```
