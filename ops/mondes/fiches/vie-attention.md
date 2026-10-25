# Attention

Le calcul précédant un acte dépose de la chaleur ; les actes sur sol chaud ont des effets locaux et laissent un écho observable.

## Identité et fonctionnement

- Identifiant : `vie-attention`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-attention/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_attn`.
- SHA-256 du fichier ELF au relevé : `31a6f27ab0e0082d105bd972bab482a069f185448dd0b2d55dca14a7f901db93`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053079**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_attn run --dir experiments/vie-attention --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-attention/series.tsv](../../../experiments/vie-attention/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56341200 |
| `pop` | 32000 |
| `n_geno` | 12244 |
| `H_geno` | 11.3053 |
| `n_raw` | 27468 |
| `persist_cum` | 573607 |
| `shadow_cum` | 1213599 |
| `new_persist` | 0 |
| `persist_alive` | 2060 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 22.70 |
| `niches_occ` | 41 |
| `stable_niches` | 11 |
| `H_behav` | 3.7114 |
| `soc_pairs` | 113 |
| `soc_mi` | 0.00619 |
| `soc_null` | -0.00641 |
| `soc_excess` | 0.01260 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 12142 |
| `n_comp` | 10243 |
| `comp_per_ind` | 13.7 |
| `comp_cum` | 864501 |
| `comp_new` | 0 |
| `comp_alive` | 145 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.719 |
| `min_ess` | 15 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

## Faits remarquables

### Diversité de comportements malgré une fenêtre sans renouvellement

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** La population de 32 000 individus comprend 12 244 génotypes, 41 niches occupées et 11 niches stables. La fenêtre contient 12 142 émissions mais zéro naissance et zéro mort.

**Portée et limites.** L’activité continue alors que la démographie est stationnaire sur cette fenêtre. Les chiffres seuls ne montrent pas que la chaleur a causé les comportements observés ; cela demanderait une comparaison dédiée.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/attention-ligne/README.md](../../../experiments/attention-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-attention).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/attention-ligne/README.md](../../../experiments/attention-ligne/README.md).

[Index des mondes](../MONDES.md).
