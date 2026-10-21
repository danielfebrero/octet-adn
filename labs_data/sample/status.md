# Octet-ADN — état des éprouvettes

| labo | cycle | fitness | meilleure | pop fin | génome moy. (o) | moteur | tendance (30 derniers) | détails |
|---|---:|---:|---:|---:|---:|---|---|---|
| org-seeds | 40 | 20.17 | 23 | 24 | 259 | `5c732f9206` | ▅▂▁▆▃▅▄▇▅█▄▃▄▃▃▅▆▄▅▇▅▅▄▆▂▂▄▄▅▇ | clusters=22, unique=22, crossovers=40 |
| max-pop | 40 | 61.46 | 73.98 | 16 | 256 | `5c732f9206` | ████▇███▅▇▆▅▆▅▆▄▄▄▅▃▃▅▃▄▄▃▃▄▁▂ | peak=120 |
| survival | 40 | 178.4 | 191 | 7 | 255 | `5c732f9206` | ▅▇▇▄▅▅▄█▄▆▅▃█▁███▄▅█▅▂▆▃▁█▃▅▄▅ | median_age=200, mean_death_age=161.667, mean_lifespan=178.438 |
| energy-eff | 40 | 4.995 | 5.318 | 12 | 259 | `5c732f9206` | ▆▂▇▄▆▆▄▇▃▃▆▃▃█▁▄▂▄▁▆▆▇▁▇▇▅▃▃██ | energy_eaten=14680 |
| self-rewrite | 40 | 0.1155 | 0.2627 | 25 | 254 | `5c732f9206` | ▂▃▄▄▄█▄▆▁▃▁▄▁▇▅▂▄▃▄▂▃▃▃▅▅▆▃▄▄▃ | store_writes=1528, useful_stores=1495 |
| emit-echo | 40 | 0.9674 | 0.9674 | 120 | 258 | `5c732f9206` | ▃▆▅▂▂▁▇▇▇▄▂▃▆▄▅▂▃▆▆▇▁▇██▇█▄▇▇█ | correct=22648, emits=22687, accuracy=0.998281 |
| emit-dialog | 40 | 0.6541 | 0.6852 | 120 | 257 | `5c732f9206` | █▆▅▅▄▃█▄▆▂▆▄▇▅▂█▇▆▆▆▄▁█▄▄▆▇█▆▅ | correct=14904, emits=21155, accuracy=0.704514 |
| predict-walk | 40 | 0.1045 | 0.342 | 19 | 251 | `5c732f9206` | ▇▆▅▇▆▅▄▅▇▇▇▇█▂▃▃▁▁▁▁▁▂▁▂▁▂▁▃▁▃ | correct=28, emits=268, accuracy=0.104478, baseline=0.316583, csv=0 |
| predict-stock | 40 | 0.4863 | 0.5526 | 19 | 274 | `5c732f9206` | ▅▇█▅▄▇▄▆▅▆▅█▁▃▇▁▆▆▆▆▆▅█▃▇▆▇▄▅▆ | correct=496, emits=1020, accuracy=0.486275, baseline=0.537688, csv=0 |
| predator-prey | 40 | 0.4 | 1 | 10 | 254 | `5c732f9206` | ███████▄█████▅██████▁██▄██████ | mean_prey=51.15, mean_pred=2, attacks=152 |

Dernière mise à jour : 2026-10-21T17:57:30+02:00

`moteur` = empreinte du binaire `octet-adn` : quand il est recompilé, le runner se ré-exécute et l'amélioration de la VM/ISA (la « popote ») s'applique à tous les labos dès le cycle suivant.
