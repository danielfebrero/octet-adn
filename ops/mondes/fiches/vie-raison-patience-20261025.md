# Raison — patience

Conditionner l’attente d’un paiement différé à l’information sur son délai. Monde vivant du bras info, graine 101, distinct des 200 essais contrôlés.

## Identité et fonctionnement

- Identifiant : `vie-raison-patience-20261025`.
- Famille : Raisonnement.
- Données : `experiments/vie-raison-patience-20261025/` (locales, non archivées dans Git).
- Moteur figé : `experiments/raison-build/2b003cf/raison`.
- SHA-256 du fichier ELF au relevé : `60e8a2fe626ade7cb67c678f841ea6f00406856e9f555a12496ba1551e3f8a23`.
- Conteneur : `octet-raison`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **3881860**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
/app/experiments/raison-build/2b003cf/raison run /data/patience --cycles 0 --pause-ms 1000
```

### Fondation

Manifeste local : [experiments/vie-raison-patience-20261025/manifest.json](../../../experiments/vie-raison-patience-20261025/manifest.json).

```json
{
  "tache": "patience",
  "bras": "info",
  "graine": 101,
  "elf": "experiments/raison-build/2b003cf/raison",
  "elf_sha256": "60e8a2fe626ade7cb67c678f841ea6f00406856e9f555a12496ba1551e3f8a23",
  "commit": "8a386545900fcea9752bb549289453c556edade7",
  "serie": 1024,
  "eval": 16384,
  "checkpoint": 32768,
  "pause_ms_par_64_cycles": 1000,
  "cree": "2026-10-25T15:48:00Z"
}
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-raison-patience-20261025/series.tsv](../../../experiments/vie-raison-patience-20261025/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `cycle` | 262144 |
| `pop` | 256 |
| `genotypes` | 154 |
| `naissances` | 2114764 |
| `morts` | 65 |
| `retires` | 2114699 |
| `m0` | 0.4997 |
| `m1` | 0.9512 |
| `m2` | 0.9519 |
| `m3` | -1.0000 |
| `energie` | 125.3 |

### eval.tsv

Dernière ligne complète de [experiments/vie-raison-patience-20261025/eval.tsv](../../../experiments/vie-raison-patience-20261025/eval.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `cycle` | 262144 |
| `pop` | 256 |
| `m0` | 0.508179 |
| `m1` | 0.905274 |
| `m2` | 0.870914 |
| `m3` | -1.000000 |
| `episodes` | 8192 |

## Lecture et continuité

`series.tsv` décrit la population en évolution ; `eval.tsv` mesure des clones. Le sens de m0…m3 dépend du milieu et des ablations : voir le protocole et `experiments/raison.c`. Une valeur −1 indique une mesure non applicable. Seul autrui passe le critère préenregistré de la série à dix graines ; les apparitions dans les autres milieux restent des résultats exploratoires.

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

Le conteneur reprend l’état existant. Le script de fondation de la famille crée des mondes neufs : ne pas le réexécuter pour reprendre celui-ci. Avant toute reprise, prouver l’absence du processus et conserver le même ELF et le checkpoint.

## Sources

- [experiments/raison/PROTOCOLE.md](../../../experiments/raison/PROTOCOLE.md).
- [experiments/raison/RESULTATS.md](../../../experiments/raison/RESULTATS.md).

[Index des mondes](../MONDES.md).
