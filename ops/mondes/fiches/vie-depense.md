# Dépense

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Fourche de vie-attention : au plafond de l’archipel, les carrés de recettes distinctes perdent des briques. La dépense rouvre des trous dans les infrastructures.

## Identité et fonctionnement

- Identifiant : `vie-depense`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-depense/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_depense`.
- SHA-256 du fichier ELF au relevé : `8d209ea744d30be1cd821b6f989f65e28c7f87d4877dcb6fa9d0273e57b85db2`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053087**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_depense run --dir experiments/vie-depense --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-depense/series.tsv](../../../experiments/vie-depense/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56544400 |
| `pop` | 32000 |
| `n_geno` | 10535 |
| `H_geno` | 10.9844 |
| `n_raw` | 26472 |
| `persist_cum` | 575739 |
| `shadow_cum` | 1219942 |
| `new_persist` | 0 |
| `persist_alive` | 1987 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 23.42 |
| `niches_occ` | 41 |
| `stable_niches` | 8 |
| `H_behav` | 3.3252 |
| `soc_pairs` | 84 |
| `soc_mi` | 0.01015 |
| `soc_null` | 0.01177 |
| `soc_excess` | -0.00161 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 9566 |
| `n_comp` | 8695 |
| `comp_per_ind` | 14.4 |
| `comp_cum` | 864555 |
| `comp_new` | 0 |
| `comp_alive` | 199 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.703 |
| `min_ess` | 14 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### lois.tsv

Dernière ligne complète de [experiments/vie-depense/lois.tsv](../../../experiments/vie-depense/lois.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56544400 |
| `spend` | 0 |
| `partage` | 0 |
| `marques` | 0 |
| `pompe_milli` | 1000 |
| `lois` | 0 |

## Faits remarquables

### La règle de dépense existe, son compteur est nul au relevé

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** Le monde compte 32 000 individus, 10 535 génotypes et huit niches stables. lois.tsv donne spend=0, partage=0 et marques=0, tandis que series.tsv rapporte 9 566 émissions.

**Portée et limites.** Ne pas transformer la description de la règle en événement observé : aucune dépense n’est attestée par ce compteur à cet instant. Il faudrait plusieurs relevés pour décrire sa fréquence.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/depense-ligne/README.md](../../../experiments/depense-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-depense).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/depense-ligne/README.md](../../../experiments/depense-ligne/README.md).

[Index des mondes](../MONDES.md).
