# Forces et briques

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Les EMIT déposent des briques dont les recettes viennent du germen ; des carrés mixtes irriguent et les constructions ouvrent des îles.

## Identité et fonctionnement

- Identifiant : `vie-forces-briques`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-forces-briques/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_briques`.
- SHA-256 du fichier ELF au relevé : `1d914d13ed85204ce45d4002efa9e42d5b4946776cb944de4a45f07df38f0f7b`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053071**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_briques run --dir experiments/vie-forces-briques --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-forces-briques/series.tsv](../../../experiments/vie-forces-briques/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 58230600 |
| `pop` | 32000 |
| `n_geno` | 4524 |
| `H_geno` | 8.3479 |
| `n_raw` | 14975 |
| `persist_cum` | 567072 |
| `shadow_cum` | 1201620 |
| `new_persist` | 0 |
| `persist_alive` | 1180 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 24.62 |
| `niches_occ` | 50 |
| `stable_niches` | 4 |
| `H_behav` | 2.6121 |
| `soc_pairs` | 8181 |
| `soc_mi` | -0.07973 |
| `soc_null` | -0.08585 |
| `soc_excess` | 0.00612 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 71129 |
| `n_comp` | 4286 |
| `comp_per_ind` | 15.6 |
| `comp_cum` | 864227 |
| `comp_new` | 0 |
| `comp_alive` | 109 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.906 |
| `min_ess` | 14 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 52 |

## Faits remarquables

### Émissions abondantes et quatre niches stables

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** Le relevé associe 32 000 individus, 4 524 génotypes et 71 129 émissions dans la fenêtre. Sur 50 niches occupées, quatre sont stables ; la diversité génétique H vaut 8,3479.

**Portée et limites.** Une émission permet de déposer des briques dans ce moteur ; son compteur ne donne pas le nombre de constructions encore présentes ni de carrés mixtes fonctionnels.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/briques-ligne/README.md](../../../experiments/briques-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-forces-briques).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/briques-ligne/README.md](../../../experiments/briques-ligne/README.md).

[Index des mondes](../MONDES.md).
