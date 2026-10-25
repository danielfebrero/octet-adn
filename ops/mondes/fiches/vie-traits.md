# Traits

Les caractéristiques sont découvertes dans les faits qui séparent durablement la population. Jusqu’à huit traits ; les niches sont leurs masques, plutôt qu’une grille de gestes fixée à l’avance.

## Identité et fonctionnement

- Identifiant : `vie-traits`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-traits/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_traits`.
- SHA-256 du fichier ELF au relevé : `ea3c62410f73cee2d81a42b8c7d57eb98ae792c140e45c8371fd5b2db6639af9`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053083**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_traits run --dir experiments/vie-traits --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-traits/series.tsv](../../../experiments/vie-traits/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56171600 |
| `pop` | 32000 |
| `n_geno` | 12263 |
| `H_geno` | 11.3111 |
| `n_raw` | 27478 |
| `persist_cum` | 573602 |
| `shadow_cum` | 1213589 |
| `new_persist` | 0 |
| `persist_alive` | 2058 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 22.70 |
| `niches_occ` | 242 |
| `stable_niches` | 7 |
| `H_behav` | 4.9509 |
| `soc_pairs` | 127 |
| `soc_mi` | -0.02947 |
| `soc_null` | -0.02899 |
| `soc_excess` | -0.00048 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 12133 |
| `n_comp` | 10261 |
| `comp_per_ind` | 13.7 |
| `comp_cum` | 864501 |
| `comp_new` | 0 |
| `comp_alive` | 145 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.781 |
| `min_ess` | 13 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### traits.tsv

Dernière ligne complète de [experiments/vie-traits/traits.tsv](../../../experiments/vie-traits/traits.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56171600 |
| `n_traits` | 8 |
| `tenues` | STORE=0.793,SENSE=0.867,TURN=0.819,STEP=0.387,EAT=0.796,MOV_BA=0.600,LOOP=0.891,entre_siens=0.158 |
| `essayees` | STORE=0.793,SENSE=0.867,TURN=0.819,STEP=0.387,EAT=0.796,MOV_BA=0.600,LOOP=0.891,entre_siens=0.158 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/traits-ligne/README.md](../../../experiments/traits-ligne/README.md).

[Index des mondes](../MONDES.md).
