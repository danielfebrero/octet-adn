# Q2 — empreinte E

96 octets classiques dans un champ moyen de phase. Bain thermique, queue répulsive et champ relaxé limitent les assemblages ; la projection écrit une empreinte dans un octet libre. Suivi des identités, fissions et naissances par empreinte. Bras E, graine 101. Hérédité mesurable, sans sélection adaptative démontrée.

## Identité et fonctionnement

- Identifiant : `vie-quantique2-e-20261025`.
- Famille : Quantiques.
- Données : `experiments/vie-quantique2-e-20261025/` (locales, non archivées dans Git).
- Moteur figé : `experiments/quantique-build/20261025-q2v/monde2`.
- SHA-256 du fichier ELF au relevé : `e2271bb0c0dba7785b073aec87754317dddc109f97832c3b4317c3865caaf99d`.
- Conteneur : `octet-quantique2-e`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **1669701**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
/app/experiments/quantique-build/20261025-q2v/monde2 run /data --ticks 0 --pause-ms 1000 --events 0
```

### Fondation

Manifeste local : [experiments/vie-quantique2-e-20261025/manifest.json](../../../experiments/vie-quantique2-e-20261025/manifest.json).

```json
{
  "created_at": "2026-10-25T14:13:37Z",
  "seed": 101,
  "bras": "E",
  "reglage": "--imprint 1 --cool 100 --n 96 --box 12 --grid 6 --modes 64 --period 16 --temp 0.6 --gamma 0.5 --rep 1.5 --xi 2 --rcut 5 --spec 0.5 --gain 1 --bind-r 1.4 --window 1024",
  "image": "sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587",
  "elf": "experiments/quantique-build/20261025-q2v/monde2",
  "elf_sha256": "e2271bb0c0dba7785b073aec87754317dddc109f97832c3b4317c3865caaf99d",
  "source_sha256": "c89b6cc567a9773c7bcdf4a740f5b3975591539595f23033c136140dbe963039",
  "cpus": 0.15,
  "pause_ms_per_128_ticks": 1000,
  "journal_projections": false,
  "commit": "747505a152c5817a01b2c653849efc28d29c07cf"
}
```

## Dernier état observé

### mesures.tsv

Dernière ligne complète de [experiments/vie-quantique2-e-20261025/mesures.tsv](../../../experiments/vie-quantique2-e-20261025/mesures.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `t` | 422528 |
| `ensembles` | 17 |
| `plus_grand` | 5 |
| `octets_lies` | 44 |
| `ordre_phase` | 0.975219705 |
| `naissances_spont` | 375 |
| `naissances_fission` | 22042 |
| `naissances_empreinte` | 100425 |
| `morts` | 122825 |
| `gen_max_vivante` | 172 |
| `gen_max` | 447 |
| `projections` | 2080441 |
| `empreintes` | 1697800 |
| `erreur_norme` | 2.284838985e-13 |
| `energie_cin` | 0.8976503951 |

### Héritage par empreinte

Lecture cumulative de `naissances.tsv` au relevé : 100425 naissances de type 2 avec distances valides ; somme d_parent = 867700.706938, somme d_hasard = 1181497.267159. H_e = 1 − somme(d_parent)/somme(d_hasard) = **0.265592**. Le journal évolue pendant la lecture ; ce constat d’une instance ne remplace pas les contrastes de la série contrôlée.

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

Le conteneur reprend l’état existant. Le script de fondation de la famille crée des mondes neufs : ne pas le réexécuter pour reprendre celui-ci. Avant toute reprise, prouver l’absence du processus et conserver le même ELF et le checkpoint.

Rotation des événements : `experiments/quantique/rotation-journaux.sh` ; compression et copytruncate sans toucher aux processus ni à `state.ck`.

## Sources

- [experiments/quantique2/PROTOCOLE.md](../../../experiments/quantique2/PROTOCOLE.md).
- [experiments/quantique2/RESULTATS.md](../../../experiments/quantique2/RESULTATS.md).

[Index des mondes](../MONDES.md).
