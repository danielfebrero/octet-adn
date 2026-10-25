# Jardin — monde-forces

Jardin sous les règles du monde initial : origine A = vie-monde, origine B = vie-forces. Fondation à partir de copies des sources, qui continuent indépendamment. Suivre la persistance de chaque origine.

## Identité et fonctionnement

- Identifiant : `vie-jardin-monde-forces-20261025`.
- Famille : Jardins.
- Données : `experiments/vie-jardin-monde-forces-20261025/` (locales, non archivées dans Git).
- Moteur figé : `experiments/transplant-build/a02b8fa/transplant`.
- SHA-256 du fichier ELF au relevé : `7e4cbcaeb235d60dee5386c7e51df8718b195360ce61788766d3345b53a79587`.
- Conteneur : `octet-jardins`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **385180**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
/app/experiments/transplant-build/a02b8fa/transplant vivre /data/vie-jardin-monde-forces-20261025
```

### Fondation

Manifeste local : [experiments/vie-jardin-monde-forces-20261025/manifest.json](../../../experiments/vie-jardin-monde-forces-20261025/manifest.json).

```json
{
  "jardin": "vie-jardin-monde-forces-20261025",
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

Dernière ligne complète de [experiments/vie-jardin-monde-forces-20261025/series.tsv](../../../experiments/vie-jardin-monde-forces-20261025/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 1234600 |
| `pop` | 200 |
| `n_geno` | 67 |
| `H_geno` | 5.2014 |
| `n_raw` | 200 |
| `persist_cum` | 2597 |
| `shadow_cum` | 1926 |
| `new_persist` | 1 |
| `persist_alive` | 12 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 25.43 |
| `niches_occ` | 7 |
| `stable_niches` | 3 |
| `H_behav` | 1.4359 |
| `soc_pairs` | 1121 |
| `soc_mi` | -0.23487 |
| `soc_null` | -0.24048 |
| `soc_excess` | 0.00561 |
| `births` | 218 |
| `deaths` | 218 |
| `emits` | 9820 |
| `n_comp` | 108 |
| `comp_per_ind` | 14.9 |
| `comp_cum` | 4390 |
| `comp_new` | 0 |
| `comp_alive` | 45 |
| `rep_try` | 790 |
| `rep_ok` | 218 |
| `copier_frac` | 0.984 |
| `min_ess` | 10 |
| `pred_hits` | 0 |
| `scomp_cum` | 21429 |
| `scomp_new` | 2 |
| `scomp_alive` | 17 |

### origine.tsv

Dernière ligne complète de [experiments/vie-jardin-monde-forces-20261025/origine.tsv](../../../experiments/vie-jardin-monde-forces-20261025/origine.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `t` | 1234600 |
| `origine_A` | 0 |
| `origine_B` | 200 |
| `inconnue` | 0 |
| `pop` | 200 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

Le conteneur reprend l’état existant. Le script de fondation de la famille crée des mondes neufs : ne pas le réexécuter pour reprendre celui-ci. Avant toute reprise, prouver l’absence du processus et conserver le même ELF et le checkpoint.

## Sources

- [experiments/transplant/PROTOCOLE.md](../../../experiments/transplant/PROTOCOLE.md).
- [experiments/transplant/RESULTATS.md](../../../experiments/transplant/RESULTATS.md).

[Index des mondes](../MONDES.md).
