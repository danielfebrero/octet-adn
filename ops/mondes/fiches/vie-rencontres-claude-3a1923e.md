# Rencontres Claude

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Profil all du moteur claude-3a1923e, un petit monde indépendant avec échanges, nurserie et rendez-vous. Les profils capture/parenté/sénescence sont trois autres instances.

## Identité et fonctionnement

- Identifiant : `vie-rencontres-claude-3a1923e`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-rencontres-claude-3a1923e/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/claude-3a1923e/terrarium`.
- SHA-256 du fichier ELF au relevé : `9a07a9437967188d016e27fbed78962f06cd7658f1780f9536df7c9a61dd2d1e`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2526960**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/claude-3a1923e/terrarium run --dir experiments/vie-rencontres-claude-3a1923e --ticks 0 --profile all --islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --migrate 0 --seed 101
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-rencontres-claude-3a1923e/series.tsv](../../../experiments/vie-rencontres-claude-3a1923e/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 11479800 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 297.39 |
| `len_max` | 299 |
| `coding_mean` | 206.79 |
| `niches_occ` | 2 |
| `stable_niches` | 1 |
| `H_behav` | 0.0969 |
| `soc_pairs` | 0 |
| `soc_mi` | 0.00000 |
| `soc_null` | 0.00000 |
| `soc_excess` | 0.00000 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 234 |
| `n_comp` | 25343 |
| `comp_per_ind` | 161.7 |
| `comp_cum` | 387819 |
| `comp_new` | 4 |
| `comp_alive` | 805 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 14078 |
| `scomp_new` | 0 |
| `scomp_alive` | 1966 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-rencontres-claude-3a1923e/rencontres.tsv](../../../experiments/vie-rencontres-claude-3a1923e/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 11479800 |
| `contacts` | 192017040 |
| `intra_contacts` | 190915884 |
| `inter_contacts` | 1101156 |
| `exchanges` | 23868009 |
| `exchange_at_cap` | 23647129 |
| `bytes_changed` | 364790920 |
| `eggs` | 3829 |
| `hatched` | 3797 |
| `crossed_children` | 3774 |
| `birth_no_death` | 1352 |
| `cap_refused` | 361070766 |
| `place_refused` | 0 |
| `energy_refused` | 202 |
| `season_refused` | 430488 |
| `reads` | 3287933 |
| `moves` | 1050202 |
| `move_energy` | 1050202 |
| `pending` | 32 |
| `exchange_cooldown_refused` | 121728841 |
| `partner_refused` | 791 |
| `naissance_cooldown_refused` | 8 |
| `food_refused` | 5298 |
| `queue_refused` | 277069 |

## Faits remarquables

### Échanges au plafond et enfants croisés attestés

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** rencontres.tsv compte 23868009 échanges, dont 23647129 au plafond, et 364790920 octets changés. Sur 3797 éclosions, 3774 enfants différaient des deux ADN parentaux avant mutation ; 1352 éclosions ont eu lieu sans mort sur la même île au même tick. La population au relevé est 160, avec 2 niches occupées et 1 stables.

**Portée et limites.** Les contacts sont des observations répétées de paires physiques, pas des partenaires distincts ni des sociétés. Les compteurs sont cumulatifs ; les durées et moteurs diffèrent entre ces trois mondes, ce qui interdit de classer leur efficacité par les totaux bruts.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/rencontres-ligne/README.md](../../../experiments/rencontres-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-rencontres-claude-3a1923e).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/README.md](../../../experiments/rencontres-ligne/README.md).

[Index des mondes](../MONDES.md).
