# Capture

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Profil capture : un porteur ATTACK prélève jusqu’à huit octets d’un voisin différent dans son soma et son germen. Suivre la circulation et la distance ADN, pas seulement le nombre de captures.

## Identité et fonctionnement

- Identifiant : `vie-capture-3a1923e`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-capture-3a1923e/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/claude-3a1923e/terrarium`.
- SHA-256 du fichier ELF au relevé : `9a07a9437967188d016e27fbed78962f06cd7658f1780f9536df7c9a61dd2d1e`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2526969**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/claude-3a1923e/terrarium run --dir experiments/vie-capture-3a1923e --ticks 0 --profile capture --islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --migrate 0 --seed 101
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-capture-3a1923e/series.tsv](../../../experiments/vie-capture-3a1923e/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 10419600 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 409.40 |
| `len_max` | 426 |
| `coding_mean` | 284.04 |
| `niches_occ` | 30 |
| `stable_niches` | 10 |
| `H_behav` | 4.1222 |
| `soc_pairs` | 8276 |
| `soc_mi` | -0.47168 |
| `soc_null` | -0.45560 |
| `soc_excess` | -0.01607 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 8153 |
| `n_comp` | 24234 |
| `comp_per_ind` | 222.4 |
| `comp_cum` | 378151 |
| `comp_new` | 0 |
| `comp_alive` | 2096 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 271754 |
| `scomp_new` | 0 |
| `scomp_alive` | 3676 |

### vivre.tsv

Dernière ligne complète de [experiments/vie-capture-3a1923e/vivre.tsv](../../../experiments/vie-capture-3a1923e/vivre.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 10419600 |
| `pop` | 160 |
| `captures` | 11646660 |
| `capture_bytes` | 47114360 |
| `capture_refused` | 14715 |
| `capture_dist_mean` | 289.792 |
| `donations` | 0 |
| `donated` | 0 |
| `donation_refused` | 0 |
| `old_deaths` | 0 |
| `give_byte` | 40.119 |
| `life_byte` | 40.763 |
| `neutral_byte` | 31.281 |
| `frac_attack` | 0.0000 |
| `frac_xor` | 1.0000 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-capture-3a1923e/rencontres.tsv](../../../experiments/vie-capture-3a1923e/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 10419600 |
| `contacts` | 175968506 |
| `intra_contacts` | 153999750 |
| `inter_contacts` | 21968756 |
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

### Circulation massive d’octets par capture

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** vivre.tsv conserve 11 646 660 captures et 47 114 360 octets capturés, avec 14 715 refus. La distance ADN moyenne mesurée vaut 289,792 ; les 160 individus ont chacun un génotype distinct. Le génome moyen mesure 409,40 octets.

**Portée et limites.** Ces captures sont cumulatives : frac_attack=0 au relevé n’annule pas leur historique. Capturer de l’ADN ne prouve ni son utilité ni une adaptation par transfert horizontal.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/rencontres-ligne/VIVRE.md](../../../experiments/rencontres-ligne/VIVRE.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-capture-3a1923e).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/VIVRE.md](../../../experiments/rencontres-ligne/VIVRE.md).

[Index des mondes](../MONDES.md).
