# Rencontres b129054c

Profil all : échanges de segments, œufs en nurserie et pas vers les marques. Version historique à fenêtre non bouclée et repos 64 ; conserver son ELF propre.

## Identité et fonctionnement

- Identifiant : `vie-rencontres-b129054c`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-rencontres-b129054c/` (locales, non archivées dans Git).
- Moteur figé : `experiments/rencontres-build/b129054c/terrarium`.
- SHA-256 du fichier ELF au relevé : `2492683cd8ac0fcc1926ab45086cb86b1da6789f67588d737078bcf1a2e1e9ca`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053098**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/rencontres-build/b129054c/terrarium run --dir experiments/vie-rencontres-b129054c --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-rencontres-b129054c/series.tsv](../../../experiments/vie-rencontres-b129054c/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 43377600 |
| `pop` | 160 |
| `n_geno` | 160 |
| `H_geno` | 7.3219 |
| `n_raw` | 160 |
| `persist_cum` | 0 |
| `shadow_cum` | 0 |
| `new_persist` | 0 |
| `persist_alive` | 0 |
| `len_mean` | 281.04 |
| `len_max` | 290 |
| `coding_mean` | 228.64 |
| `niches_occ` | 4 |
| `stable_niches` | 2 |
| `H_behav` | 0.8178 |
| `soc_pairs` | 1831 |
| `soc_mi` | -0.07352 |
| `soc_null` | -0.07003 |
| `soc_excess` | -0.00349 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 4500 |
| `n_comp` | 26563 |
| `comp_per_ind` | 181.4 |
| `comp_cum` | 1399026 |
| `comp_new` | 6 |
| `comp_alive` | 476 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | -1.000 |
| `min_ess` | -1 |
| `pred_hits` | 0 |
| `scomp_cum` | 50639 |
| `scomp_new` | 0 |
| `scomp_alive` | 4375 |

### rencontres.tsv

Dernière ligne complète de [experiments/vie-rencontres-b129054c/rencontres.tsv](../../../experiments/vie-rencontres-b129054c/rencontres.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 43377600 |
| `contacts` | 589738726 |
| `intra_contacts` | 583794676 |
| `inter_contacts` | 5944050 |
| `exchanges` | 45009191 |
| `exchange_at_cap` | 44784772 |
| `bytes_changed` | 696641240 |
| `eggs` | 1945 |
| `hatched` | 1913 |
| `crossed_children` | 1904 |
| `birth_no_death` | 1020 |
| `cap_refused` | 1378407172 |
| `place_refused` | 0 |
| `energy_refused` | 360 |
| `season_refused` | 1626660 |
| `reads` | 1731363997 |
| `moves` | 476810653 |
| `move_energy` | 476810653 |
| `pending` | 32 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/rencontres-ligne/README.md](../../../experiments/rencontres-ligne/README.md).

[Index des mondes](../MONDES.md).
