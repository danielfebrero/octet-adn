# Forces

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Fourche du monde initial : prédation portée par ATTACK, repousse réduite et expansion douce de l’archipel.

## Identité et fonctionnement

- Identifiant : `vie-forces`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-forces/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_run`.
- SHA-256 du fichier ELF au relevé : `d91bd4d8b929c500258581579bd05e18f61ccbf52524affb5157251cf0de6d78`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053067**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_run run --dir experiments/vie-forces --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-forces/series.tsv](../../../experiments/vie-forces/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 143743200 |
| `pop` | 2207 |
| `n_geno` | 477 |
| `H_geno` | 7.6666 |
| `n_raw` | 1175 |
| `persist_cum` | 1882833 |
| `shadow_cum` | 1703287 |
| `new_persist` | 1 |
| `persist_alive` | 55 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 28.14 |
| `niches_occ` | 21 |
| `stable_niches` | 3 |
| `H_behav` | 0.9157 |
| `soc_pairs` | 6 |
| `soc_mi` | 0.00000 |
| `soc_null` | -0.16667 |
| `soc_excess` | 0.16667 |
| `births` | 6262 |
| `deaths` | 6383 |
| `emits` | 1924 |
| `n_comp` | 716 |
| `comp_per_ind` | 18.9 |
| `comp_cum` | 2499815 |
| `comp_new` | 5 |
| `comp_alive` | 153 |
| `rep_try` | 18683 |
| `rep_ok` | 6249 |
| `copier_frac` | 0.953 |
| `min_ess` | 15 |
| `pred_hits` | 0 |
| `scomp_cum` | 6695971 |
| `scomp_new` | 19 |
| `scomp_alive` | 119 |

## Faits remarquables

### Renouvellement intense sous les règles de prédation

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** À 2 207 vivants pour 477 génotypes, la fenêtre rapporte 6 262 naissances et 6 383 morts. H=7,6666 et trois niches stables sont encore mesurées. L’excès social de 0,16667 repose sur seulement six paires.

**Portée et limites.** Un excès élevé sur six paires ne suffit pas à conclure à une société. pred_hits=0 dans cette fenêtre ne signifie pas que la prédation n’a jamais eu lieu.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/forces-ligne/README.md](../../../experiments/forces-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-forces).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/forces-ligne/README.md](../../../experiments/forces-ligne/README.md).

[Index des mondes](../MONDES.md).
