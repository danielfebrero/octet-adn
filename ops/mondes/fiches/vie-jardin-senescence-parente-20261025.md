# Jardin — senescence-parente

Jardin sous les règles de sénescence : origine A = vie-senescence-3a1923e, origine B = vie-parente-3a1923e. Fondation à partir de copies des sources, qui continuent indépendamment. Suivre la persistance de chaque origine.

## Identité et fonctionnement

- Identifiant : `vie-jardin-senescence-parente-20261025`.
- Famille : Jardins.
- Données : `experiments/vie-jardin-senescence-parente-20261025/` (locales, non archivées dans Git).
- Moteur figé : `experiments/transplant-build/a02b8fa/transplant`.
- SHA-256 du fichier ELF au relevé : `7e4cbcaeb235d60dee5386c7e51df8718b195360ce61788766d3345b53a79587`.
- Conteneur : `octet-jardins`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **385179**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
/app/experiments/transplant-build/a02b8fa/transplant vivre /data/vie-jardin-senescence-parente-20261025
```

### Fondation

Manifeste local : [experiments/vie-jardin-senescence-parente-20261025/manifest.json](../../../experiments/vie-jardin-senescence-parente-20261025/manifest.json).

```json
{
  "jardin": "vie-jardin-senescence-parente-20261025",
  "elf": "experiments/transplant-build/a02b8fa/transplant",
  "elf_sha256": "7e4cbcaeb235d60dee5386c7e51df8718b195360ce61788766d3345b53a79587",
  "commit": "a02b8fa2b2712d35ed87de032885c134a734a93b",
  "graine": 101,
  "checkpoint": 20000,
  "cree": "2026-10-25T16:23:05Z"
}
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-jardin-senescence-parente-20261025/series.tsv](../../../experiments/vie-jardin-senescence-parente-20261025/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 549400 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 754.34 |
| `len_max` | 1024 |
| `coding_mean` | 577.74 |
| `niches_occ` | 35 |
| `stable_niches` | 10 |
| `H_behav` | 4.4759 |
| `soc_pairs` | 7049 |
| `soc_mi` | -0.34636 |
| `soc_null` | -0.36440 |
| `soc_excess` | 0.01804 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 9169 |
| `n_comp` | 33930 |
| `comp_per_ind` | 447.9 |
| `comp_cum` | 461 |
| `comp_new` | 0 |
| `comp_alive` | 8877 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 1269 |
| `scomp_new` | 0 |
| `scomp_alive` | 10549 |

### origine.tsv

Dernière ligne complète de [experiments/vie-jardin-senescence-parente-20261025/origine.tsv](../../../experiments/vie-jardin-senescence-parente-20261025/origine.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `t` | 549400 |
| `origine_A` | 107 |
| `origine_B` | 53 |
| `inconnue` | 0 |
| `pop` | 160 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

Le conteneur reprend l’état existant. Le script de fondation de la famille crée des mondes neufs : ne pas le réexécuter pour reprendre celui-ci. Avant toute reprise, prouver l’absence du processus et conserver le même ELF et le checkpoint.

## Sources

- [experiments/transplant/PROTOCOLE.md](../../../experiments/transplant/PROTOCOLE.md).
- [experiments/transplant/RESULTATS.md](../../../experiments/transplant/RESULTATS.md).

[Index des mondes](../MONDES.md).
