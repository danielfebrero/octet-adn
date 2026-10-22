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

## Résultats (`results.txt`, `./experiments/mi_detector controls`, ~16 s)
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
- **Positif** : le détecteur sépare nettement le vivant (seeds + évolués) du bruit et de l'ELF
  (≈ 60 % des évolués au-dessus du p95 du bruit, contre 2.5 % pour l'ELF), et survit au contrôle
  de composition (mélangés) et au contrôle de survie.
- **Négatif** : la composante **mémoire / prédictive est nulle ou négative partout** (médiane évoluée
  −0.065, AUC 0.27 vs bruit) : aucun génome, seed ou évolué, n'utilise de façon détectable le
  stimulus passé au-delà du présent. Ce qui est détecté, c'est de la **réactivité**, pas de la prédiction.
- ~40 % des évolués scorent bas : lignées qui meurent seules (commons, code-repair…) ou dont la niche
  est sociale/labo — un monde vide n'excite pas leur comportement. Faux positifs max : un mélangé de
  predator-prey (0.34), un bruit (0.21).
- Fuite possible : `F_t` dépend en boucle fermée des actions passées ; borné empiriquement par les
  contrôles bruit/mélangés (p95 = 0.124).

## Suite
Labo « prédiction » où le stimulus passé détermine la nourriture future (pression sur `I_mem`),
puis réutiliser `I_mem` comme fitness ; tester le relais sur un monde peuplé (signaux d'autrui).

## Usage
```
make experiments/mi_detector
./experiments/mi_detector controls [dossier_ck] [elf]
./experiments/mi_detector mi seed:echo fichier.ck
./experiments/mi_detector genes seed:echo
./experiments/mi_detector selftest
```
