# Parenté

Profil parente : don d’énergie à un voisin affamé, selon une propension portée par le dernier octet. Comparer give_byte au témoin neutral_byte sur plusieurs relevés.

## Identité et fonctionnement

- Identifiant : `vie-parente-3a1923e`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-parente-3a1923e/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/claude-3a1923e/terrarium`.
- SHA-256 du fichier ELF au relevé : `9a07a9437967188d016e27fbed78962f06cd7658f1780f9536df7c9a61dd2d1e`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2527013**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/claude-3a1923e/terrarium run --dir experiments/vie-parente-3a1923e --ticks 0 --profile parente --islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --migrate 0 --seed 101
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-parente-3a1923e/series.tsv](../../../experiments/vie-parente-3a1923e/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 13786600 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 260.82 |
| `len_max` | 291 |
| `coding_mean` | 151.43 |
| `niches_occ` | 36 |
| `stable_niches` | 7 |
| `H_behav` | 4.0382 |
| `soc_pairs` | 10443 |
| `soc_mi` | -0.31726 |
| `soc_null` | -0.32179 |
| `soc_excess` | 0.00453 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 12894 |
| `n_comp` | 10850 |
| `comp_per_ind` | 114.3 |
| `comp_cum` | 123 |
| `comp_new` | 0 |
| `comp_alive` | 1698 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 660 |
| `scomp_new` | 0 |
| `scomp_alive` | 1109 |

### vivre.tsv

Dernière ligne complète de [experiments/vie-parente-3a1923e/vivre.tsv](../../../experiments/vie-parente-3a1923e/vivre.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 13786600 |
| `pop` | 160 |
| `captures` | 0 |
| `capture_bytes` | 0 |
| `capture_refused` | 0 |
| `capture_dist_mean` | 0.000 |
| `donations` | 89932 |
| `donated` | 899320 |
| `donation_refused` | 53848 |
| `old_deaths` | 0 |
| `give_byte` | 155.469 |
| `life_byte` | 199.312 |
| `neutral_byte` | 170.806 |
| `frac_attack` | 1.0000 |
| `frac_xor` | 1.0000 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-parente-3a1923e/rencontres.tsv](../../../experiments/vie-parente-3a1923e/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 13786600 |
| `contacts` | 204461978 |
| `intra_contacts` | 204461313 |
| `inter_contacts` | 665 |
| `exchanges` | 0 |
| `exchange_at_cap` | 0 |
| `bytes_changed` | 0 |
| `eggs` | 0 |
| `hatched` | 0 |
| `crossed_children` | 0 |
| `birth_no_death` | 0 |
| `cap_refused` | 0 |
| `place_refused` | 0 |
| `energy_refused` | 0 |
| `season_refused` | 0 |
| `reads` | 0 |
| `moves` | 0 |
| `move_energy` | 0 |
| `pending` | 0 |
| `exchange_cooldown_refused` | 0 |
| `partner_refused` | 0 |
| `naissance_cooldown_refused` | 0 |
| `food_refused` | 0 |
| `queue_refused` | 0 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/VIVRE.md](../../../experiments/rencontres-ligne/VIVRE.md).

[Index des mondes](../MONDES.md).
