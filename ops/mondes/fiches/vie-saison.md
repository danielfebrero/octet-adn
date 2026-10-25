# Saison

Fourche de vie-forces-societes : saison douce sur la repousse, les fermes et les technologies (amplitude 0,35, période 24 000).

## Identité et fonctionnement

- Identifiant : `vie-saison`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-saison/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_saison`.
- SHA-256 du fichier ELF au relevé : `8d209ea744d30be1cd821b6f989f65e28c7f87d4877dcb6fa9d0273e57b85db2`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053089**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_saison run --dir experiments/vie-saison --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-saison/series.tsv](../../../experiments/vie-saison/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56926800 |
| `pop` | 32000 |
| `n_geno` | 13186 |
| `H_geno` | 11.8580 |
| `n_raw` | 28288 |
| `persist_cum` | 576741 |
| `shadow_cum` | 1223236 |
| `new_persist` | 0 |
| `persist_alive` | 2415 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 23.52 |
| `niches_occ` | 35 |
| `stable_niches` | 4 |
| `H_behav` | 2.2828 |
| `soc_pairs` | 36 |
| `soc_mi` | -0.01694 |
| `soc_null` | 0.02296 |
| `soc_excess` | -0.03990 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 10063 |
| `n_comp` | 12840 |
| `comp_per_ind` | 14.4 |
| `comp_cum` | 864551 |
| `comp_new` | 0 |
| `comp_alive` | 204 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.734 |
| `min_ess` | 15 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### lois.tsv

Dernière ligne complète de [experiments/vie-saison/lois.tsv](../../../experiments/vie-saison/lois.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56926800 |
| `spend` | 0 |
| `partage` | 0 |
| `marques` | 0 |
| `pompe_milli` | 892 |
| `lois` | 0 |

## Faits remarquables

### La pompe saisonnière est sous son maximum

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** Le facteur pompe_milli=892 correspond à 89,2 % du niveau nominal au relevé. Le mécanisme module les apports des fermes et de la technologie, et pas uniquement la repousse de base.

**Portée et limites.** Une phase de saison ne montre pas à elle seule une adaptation saisonnière. Comparer plusieurs phases et un témoin serait nécessaire pour attribuer un effet à ce mécanisme.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/saison-ligne/README.md](../../../experiments/saison-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-saison).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/saison-ligne/README.md](../../../experiments/saison-ligne/README.md).

[Index des mondes](../MONDES.md).
