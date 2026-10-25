# Q1 — période 64

64 octets classiques en 3D, champ moyen complexe fini. Les liens persistants définissent les assemblages ; une boucle réciproque ensemble–point déclenche une projection après 64 ticks. Graine 101. La valeur projetée reste sans effet sur les octets ; ni reproduction ni filiation dans Q1.

## Identité et fonctionnement

- Identifiant : `vie-quantique-p64-20261025`.
- Famille : Quantiques.
- Données : `experiments/vie-quantique-p64-20261025/` (locales, non archivées dans Git).
- Moteur figé : `experiments/quantique-build/20261025-q1/monde`.
- SHA-256 du fichier ELF au relevé : `2b215fa5d5c5ad1e0359a7d0db263d4022b3234ea9418feb36a680330b603a93`.
- Conteneur : `octet-quantique-p64`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **4149422**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
/app/experiments/quantique-build/20261025-q1/monde run /data --ticks 0 --pause-ms 1000
```

### Fondation

Manifeste local : [experiments/vie-quantique-p64-20261025/manifest.json](../../../experiments/vie-quantique-p64-20261025/manifest.json).

```json
{
  "created_at": "2026-10-25T12:51:20.882975+00:00",
  "seed": 101,
  "period": 64,
  "octets": 64,
  "modes": 256,
  "grid": 4,
  "dt": 0.02,
  "image": "sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587",
  "elf": "experiments/quantique-build/20261025-q1/monde",
  "pause_ms_per_128_ticks": 1000,
  "cpus": 0.15,
  "commit": "a289f8f44502935714898621c0378205a409036d",
  "sha256": {
    "experiments/quantique-build/20261025-q1/monde": "2b215fa5d5c5ad1e0359a7d0db263d4022b3234ea9418feb36a680330b603a93",
    "experiments/quantique/monde.cpp": "8956923cf0cbe78d442add4b91952ba0440e7b2b55bef78ca50501b6cc617b29",
    "experiments/quantique/PROTOCOLE.md": "c8f2c4e9d9752837d35a14fa7826cee952b27aade45677fd2f0ce700d9206345"
  }
}
```

## Dernier état observé

### mesures.tsv

Dernière ligne complète de [experiments/vie-quantique-p64-20261025/mesures.tsv](../../../experiments/vie-quantique-p64-20261025/mesures.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `t` | 816000 |
| `projections` | 87 |
| `liens` | 1024 |
| `plus_grand` | 61 |
| `maximum_assemblage` | 64 |
| `maximum_boucle` | 64 |
| `erreur_norme` | 4.1493009028670258e-09 |
| `energie_cinetique` | 475.25735597245574 |

## Lecture et continuité

Suivre t, projections, liens, tailles des assemblages et erreur_norme. Une dérive de norme fragilise l’interprétation du long terme, même si le processus continue ; ne pas la corriger en remplaçant son ELF. Le mot quantique désigne ce modèle hybride, pas une physique quantique complète.

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

Le conteneur reprend l’état existant. Le script de fondation de la famille crée des mondes neufs : ne pas le réexécuter pour reprendre celui-ci. Avant toute reprise, prouver l’absence du processus et conserver le même ELF et le checkpoint.

Rotation des événements : `experiments/quantique/rotation-journaux.sh` ; compression et copytruncate sans toucher aux processus ni à `state.ck`.

## Sources

- [experiments/quantique/PROTOCOLE.md](../../../experiments/quantique/PROTOCOLE.md).
- [experiments/quantique/RESULTATS.md](../../../experiments/quantique/RESULTATS.md).

[Index des mondes](../MONDES.md).
