# Rencontres Sonnet

Profil all, version Sonnet : échanges à fenêtre bouclée, repos 32 et colonnes de refus. Instance indépendante du premier monde rencontres.

## Identité et fonctionnement

- Identifiant : `vie-rencontres-sonnet-41ab85e`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-rencontres-sonnet-41ab85e/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/sonnet-41ab85e/terrarium`.
- SHA-256 du fichier ELF au relevé : `73f3c60b7a823c3d780a56792165eb6a93e315507d9a7cde7065ef6926389ae6`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053101**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/sonnet-41ab85e/terrarium run --dir experiments/vie-rencontres-sonnet-41ab85e --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-rencontres-sonnet-41ab85e/series.tsv](../../../experiments/vie-rencontres-sonnet-41ab85e/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 35724600 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 306.00 |
| `len_max` | 306 |
| `coding_mean` | 174.07 |
| `niches_occ` | 11 |
| `stable_niches` | 2 |
| `H_behav` | 1.5676 |
| `soc_pairs` | 3142 |
| `soc_mi` | -0.40862 |
| `soc_null` | -0.44549 |
| `soc_excess` | 0.03687 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 5417 |
| `n_comp` | 16555 |
| `comp_per_ind` | 132.4 |
| `comp_cum` | 1594405 |
| `comp_new` | 8 |
| `comp_alive` | 373 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 140664 |
| `scomp_new` | 0 |
| `scomp_alive` | 3398 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-rencontres-sonnet-41ab85e/rencontres.tsv](../../../experiments/vie-rencontres-sonnet-41ab85e/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 35724600 |
| `contacts` | 544231989 |
| `intra_contacts` | 544203292 |
| `inter_contacts` | 28697 |
| `exchanges` | 72848423 |
| `exchange_at_cap` | 72418105 |
| `bytes_changed` | 1135094092 |
| `eggs` | 55860 |
| `hatched` | 55828 |
| `crossed_children` | 54759 |
| `birth_no_death` | 2428 |
| `cap_refused` | 1123153230 |
| `place_refused` | 1 |
| `energy_refused` | 3535 |
| `season_refused` | 1339668 |
| `reads` | 1426521950 |
| `moves` | 343790532 |
| `move_energy` | 343790532 |
| `pending` | 32 |
| `exchange_cooldown_refused` | 351989207 |
| `partner_refused` | 7930 |
| `naissance_cooldown_refused` | 56 |
| `food_refused` | 52761 |
| `queue_refused` | 776500 |

## Faits remarquables

### Échanges au plafond et enfants croisés attestés

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** rencontres.tsv compte 72848423 échanges, dont 72418105 au plafond, et 1135094092 octets changés. Sur 55828 éclosions, 54759 enfants différaient des deux ADN parentaux avant mutation ; 2428 éclosions ont eu lieu sans mort sur la même île au même tick. La population au relevé est 160, avec 11 niches occupées et 2 stables.

**Portée et limites.** Les contacts sont des observations répétées de paires physiques, pas des partenaires distincts ni des sociétés. Les compteurs sont cumulatifs ; les durées et moteurs diffèrent entre ces trois mondes, ce qui interdit de classer leur efficacité par les totaux bruts.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/rencontres-ligne/README.md](../../../experiments/rencontres-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-rencontres-sonnet-41ab85e).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/README.md](../../../experiments/rencontres-ligne/README.md).

[Index des mondes](../MONDES.md).
