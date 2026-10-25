# Lecture

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Fourche du tronc dont l’ABI permet aussi de lire les briques. Les plugins lecture et trace peuvent observer et marquer les constructions ; un comptage seul ne prouve pas un usage évolué.

## Identité et fonctionnement

- Identifiant : `vie-lecture`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-lecture/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_lecture`.
- SHA-256 du fichier ELF au relevé : `6181ecbb42efd0cf0b2253f477f188111df0a3a0b600a7a3a2db77f190b680e8`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053093**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_lecture run --dir experiments/vie-lecture --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-lecture/series.tsv](../../../experiments/vie-lecture/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56367200 |
| `pop` | 32000 |
| `n_geno` | 10230 |
| `H_geno` | 10.7698 |
| `n_raw` | 26150 |
| `persist_cum` | 575799 |
| `shadow_cum` | 1220136 |
| `new_persist` | 0 |
| `persist_alive` | 1929 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 23.54 |
| `niches_occ` | 191 |
| `stable_niches` | 5 |
| `H_behav` | 4.2776 |
| `soc_pairs` | 349 |
| `soc_mi` | -0.07377 |
| `soc_null` | -0.12621 |
| `soc_excess` | 0.05243 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 14217 |
| `n_comp` | 8335 |
| `comp_per_ind` | 14.5 |
| `comp_cum` | 864552 |
| `comp_new` | 0 |
| `comp_alive` | 192 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.766 |
| `min_ess` | 15 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### traits.tsv

Dernière ligne complète de [experiments/vie-lecture/traits.tsv](../../../experiments/vie-lecture/traits.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56367200 |
| `n_traits` | 8 |
| `tenues` | STORE=0.919,LOAD=0.925,TURN=0.940,STEP=0.916,EAT=0.866,MOV_BA=0.806,ATTACK=0.364,entre_siens=0.438 |
| `essayees` | STORE=0.919,LOAD=0.925,TURN=0.940,STEP=0.916,EAT=0.866,MOV_BA=0.806,ATTACK=0.364,entre_siens=0.438 |

### lois.tsv

Dernière ligne complète de [experiments/vie-lecture/lois.tsv](../../../experiments/vie-lecture/lois.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56367200 |
| `spend` | 0 |
| `partage` | 26029 |
| `marques` | 0 |
| `pompe_milli` | 1000 |
| `lois` | 7 |

Plugins présents au relevé (la présence du fichier ne prouve pas à elle seule son activation) : `competition.so`, `heritage.so`, `lecture.so`, `memoire.so`, `osmo.so`, `renaissance.so`, `trace.so`.

## Faits remarquables

### Sept lois comptées, sans marque dans le relevé

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** lois.tsv donne lois=7, partage=26 029 et marques=0. Le monde compte 32 000 individus, 10 230 génotypes et cinq niches stables sur 191 occupées. L’excès social est de 0,05243 sur 349 paires.

**Portée et limites.** La possibilité de lire ou marquer les briques ne démontre pas un usage évolué de cette lecture. L’excès social et la présence des fichiers lecture.so/trace.so ne suffisent pas à cette attribution.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/tronc-ligne/README.md](../../../experiments/tronc-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-lecture).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/tronc-ligne/README.md](../../../experiments/tronc-ligne/README.md).

[Index des mondes](../MONDES.md).
