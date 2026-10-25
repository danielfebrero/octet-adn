# Causes

Fourche du monde initial avec forces 2 : YIELD producteur, attraction AND/ADD, répulsion XOR et drain SUB.

## Identité et fonctionnement

- Identifiant : `vie-causes`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-causes/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_run`.
- SHA-256 du fichier ELF au relevé : `d91bd4d8b929c500258581579bd05e18f61ccbf52524affb5157251cf0de6d78`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053069**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_run run --dir experiments/vie-causes --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-causes/series.tsv](../../../experiments/vie-causes/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 41665400 |
| `pop` | 4000 |
| `n_geno` | 4 |
| `H_geno` | 0.0924 |
| `n_raw` | 27 |
| `persist_cum` | 229553 |
| `shadow_cum` | 937814 |
| `new_persist` | 0 |
| `persist_alive` | 4 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 19.99 |
| `niches_occ` | 15 |
| `stable_niches` | 3 |
| `H_behav` | 0.7735 |
| `soc_pairs` | 84 |
| `soc_mi` | -0.04180 |
| `soc_null` | -0.04644 |
| `soc_excess` | 0.00464 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 2359 |
| `n_comp` | 18 |
| `comp_per_ind` | 12.0 |
| `comp_cum` | 95050 |
| `comp_new` | 0 |
| `comp_alive` | 18 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 1.000 |
| `min_ess` | 7 |
| `pred_hits` | 0 |
| `scomp_cum` | 218657 |
| `scomp_new` | 0 |
| `scomp_alive` | 131 |

## Faits remarquables

### Beaucoup de vivants, très peu de diversité génétique

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** Les 4 000 individus se répartissent en seulement quatre génotypes ; H=0,0924 indique une répartition très déséquilibrée. La fenêtre ne rapporte ni naissance ni mort, mais 2 359 émissions.

**Portée et limites.** Cette faible diversité n’est pas une extinction ni une absence d’activité. Un seul relevé ne prouve pas que cette situation est irréversible.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/causes-ligne/README.md](../../../experiments/causes-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-causes).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/causes-ligne/README.md](../../../experiments/causes-ligne/README.md).

[Index des mondes](../MONDES.md).
