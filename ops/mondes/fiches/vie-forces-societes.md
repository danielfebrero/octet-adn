# Forces et sociétés

Monde à briques et mesure des dépendances sociales entre voisins. Une corrélation brute ne suffit pas : lire paires, témoin et excès, ainsi que les événements societe_intra/inter.

## Identité et fonctionnement

- Identifiant : `vie-forces-societes`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-forces-societes/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_soc`.
- SHA-256 du fichier ELF au relevé : `71961011dfe7e863046cb051239823aa38536124e087fb9b873a6eaeb74eb7f5`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053074**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_soc run --dir experiments/vie-forces-societes --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-forces-societes/series.tsv](../../../experiments/vie-forces-societes/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56631800 |
| `pop` | 32000 |
| `n_geno` | 11491 |
| `H_geno` | 11.2980 |
| `n_raw` | 27510 |
| `persist_cum` | 577932 |
| `shadow_cum` | 1227977 |
| `new_persist` | 0 |
| `persist_alive` | 2233 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 22.36 |
| `niches_occ` | 23 |
| `stable_niches` | 4 |
| `H_behav` | 1.5135 |
| `soc_pairs` | 12 |
| `soc_mi` | 0.13718 |
| `soc_null` | 0.15269 |
| `soc_excess` | -0.01550 |
| `births` | 4 |
| `deaths` | 4 |
| `emits` | 6006 |
| `n_comp` | 11713 |
| `comp_per_ind` | 12.9 |
| `comp_cum` | 864652 |
| `comp_new` | 0 |
| `comp_alive` | 308 |
| `rep_try` | 106 |
| `rep_ok` | 4 |
| `copier_frac` | 0.797 |
| `min_ess` | 14 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

## Faits remarquables

### Le détecteur social ne donne pas un résultat positif dans cette fenêtre

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** Les 32 000 vivants conservent 11 491 génotypes et quatre niches stables. La mesure sociale utilise 12 paires : MI=0,13718, témoin=0,15269, excès=−0,01550. Quatre naissances et quatre morts sont rapportées.

**Portée et limites.** La corrélation brute est inférieure au témoin. Ce relevé ne prouve pas une société ; il ne permet pas non plus de déclarer qu’aucun événement societe_intra/inter n’a existé dans l’historique.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/briques-ligne/README.md](../../../experiments/briques-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-forces-societes).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/briques-ligne/README.md](../../../experiments/briques-ligne/README.md).

[Index des mondes](../MONDES.md).
