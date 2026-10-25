# Sénescence

Profil senescence : âge maximal = 100 + 8 × octet(len−2). Suivre les morts d’âge et life_byte face au témoin neutre ; les bras contrôlés sont documentés séparément.

## Identité et fonctionnement

- Identifiant : `vie-senescence-3a1923e`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-senescence-3a1923e/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/claude-3a1923e/terrarium`.
- SHA-256 du fichier ELF au relevé : `9a07a9437967188d016e27fbed78962f06cd7658f1780f9536df7c9a61dd2d1e`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2527018**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/claude-3a1923e/terrarium run --dir experiments/vie-senescence-3a1923e --ticks 0 --profile senescence --islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --migrate 0 --seed 101
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-senescence-3a1923e/series.tsv](../../../experiments/vie-senescence-3a1923e/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 6850800 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 994.12 |
| `len_max` | 1024 |
| `coding_mean` | 781.30 |
| `niches_occ` | 35 |
| `stable_niches` | 7 |
| `H_behav` | 4.2886 |
| `soc_pairs` | 8984 |
| `soc_mi` | -0.38852 |
| `soc_null` | -0.37683 |
| `soc_excess` | -0.01169 |
| `births` | 17 |
| `deaths` | 17 |
| `emits` | 10463 |
| `n_comp` | 78921 |
| `comp_per_ind` | 605.5 |
| `comp_cum` | 9498159 |
| `comp_new` | 126 |
| `comp_alive` | 12028 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 9771359 |
| `scomp_new` | 420 |
| `scomp_alive` | 17376 |

### vivre.tsv

Dernière ligne complète de [experiments/vie-senescence-3a1923e/vivre.tsv](../../../experiments/vie-senescence-3a1923e/vivre.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 6850800 |
| `pop` | 160 |
| `captures` | 0 |
| `capture_bytes` | 0 |
| `capture_refused` | 0 |
| `capture_dist_mean` | 0.000 |
| `donations` | 0 |
| `donated` | 0 |
| `donation_refused` | 0 |
| `old_deaths` | 2222283 |
| `give_byte` | 48.619 |
| `life_byte` | 254.431 |
| `neutral_byte` | 34.469 |
| `frac_attack` | 1.0000 |
| `frac_xor` | 1.0000 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-senescence-3a1923e/rencontres.tsv](../../../experiments/vie-senescence-3a1923e/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 6850800 |
| `contacts` | 102666054 |
| `intra_contacts` | 102649476 |
| `inter_contacts` | 16578 |
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

## Faits remarquables

### Longévité codée près du maximum et morts d’âge nombreuses

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** vivre.tsv compte 2 222 283 morts d’âge. life_byte=254,431, près du maximum 255, contre neutral_byte=34,469. La longueur moyenne atteint 994,12 octets et le maximum 1 024. La fenêtre rapporte 17 naissances et 17 morts pour 160 vivants.

**Portée et limites.** Le contraste avec l’octet neutre est descriptif sur cette instance ; il ne remplace pas les bras contrôlés. Le dernier excès social vaut −0,01169 sur 8 984 paires : il ne résume pas les événements sociaux antérieurs.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/rencontres-ligne/VIVRE.md](../../../experiments/rencontres-ligne/VIVRE.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-senescence-3a1923e).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/VIVRE.md](../../../experiments/rencontres-ligne/VIVRE.md).

[Index des mondes](../MONDES.md).
