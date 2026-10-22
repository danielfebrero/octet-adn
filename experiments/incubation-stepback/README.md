# Incuber par la durée des rencontres

## Pré-enregistrement (avant la première mesure)

Base : `c01df9c`, branche `feat/incubation-stepback`, 22 octobre 2026.

**Hypothèse jamais testée : des rencontres prolongées entre les mêmes deux
programmes permettent à une première copie de devenir une lignée plus persistante,
en laissant ses effets sur le partenaire agir avant le brassage suivant.**

La chose manquée serait une durée de résidence du partenaire. La soupe dissout
chaque rencontre après une exécution. Une modification du voisin ne retrouve donc
presque jamais son auteur à l'époque suivante. Ici, une paire reste en contact
pendant huit époques. Chaque époque garde exactement une exécution par paire,
avec des rôles gauche/droite aléatoires. Les registres sont remis à zéro comme
avant. Le moteur ne reconnaît ni copieur ni réparation pour décider des contacts.

Prédiction : durée moyenne avant première extinction **au moins ×2** contre le
brassage à chaque époque, avec un gain strict dans **au moins 13/16 paires**.
Témoin : **zéro naissance descendante sans écritures**. Un échec d'un des deux
seuils réfute la prédiction à cette échelle ; un témoin non nul invalide la sonde.
Si les deux bras atteignent trop souvent l'horizon pour départager ×2, verdict
non concluant. Aucun second mécanisme ne sera ajouté après les mesures.

Plan : les deux dominants spontanés archivés dans
`experiments/soup-replicator/results.txt` (`big_b_0`, `big_b_2`), chacun inchangé,
une seule copie dans N=1024 programmes bruités ; graines 0–7 ; trois bras
(contact 1, contact 8, contact 8 sans écritures), soit **48 runs**. Horizon
4096 époques, aucune phase d'établissement offerte. Dérive K=8 exactement reprise
de `universe_search2.c` : 90 % table, 10 % paramètre. Adressage B identique dans
tous les bras ; bruit natif de cet univers, initialement 0,00024/octet/époque.

Les flux aléatoires du brassage, des rôles, du bruit et de la dérive sont séparés.
Les trois bras d'une paire partagent les mêmes tirages, y compris les brassages
qui sont tirés puis ignorés pendant un contact. Le calcul n'utilise aucune fitness.

### Idées écartées et fichiers lus

La compartimentation simple est déjà dans `experiments/terrarium.c` et son README
(archipel). Les lois locales sont déjà proposées à la fin de
`experiments/universe-search/law-keepers/README.md` : écartées aussi. Lecture de
`src/soup.c`, des README `soup-replicator`, `soup-terrarium`, `terrarium`,
`universe-search/law-keepers`, `universe-search/rebirth`, et des moteurs
`universe_search.c`, `universe_search2.c`, `k8_survivor.c`, `rebirth.c`, plus une
recherche des rencontres/contacts dans les sources et README d'`experiments/`.
La durée des paires n'y est pas une condition expérimentale.

## Résultats

**Verdict : non. Ratio mesuré : 1,261413 ; gains : 4/16.**

Les 48 runs ont pris **29,80 secondes sur huit threads**. Horizon conservé à
4096 : aucun run n'est censuré, toutes les familles ont perdu leur dernier
copieur avant la fin de l'époque 360. Il n'a donc pas été nécessaire de couper
les époques. Sortie brute complète : [`results.txt`](results.txt).

| fondateur | durée moyenne, brassage | durée moyenne, contact 8 | ratio | gains / pertes / égalités | continuité à 4096, brassage / contact |
|---|---:|---:|---:|---:|---:|
| soupe graine 0 (`big_b_0`) | 129,000 | 128,125 | 0,993 | 1 / 1 / 6 | 0/8 / 0/8 |
| soupe graine 2 (`big_b_2`) | 101,000 | 162,000 | 1,604 | 3 / 1 / 4 | 0/8 / 0/8 |
| ensemble | **115,000** | **145,0625** | **1,261413** | **4 / 2 / 10** | **0/16 / 0/16** |

Le témoin sans écritures produit **0 naissance descendante sur ses 16 runs**,
et ne dépasse jamais la génération 0. Les bras avec écritures produisent
227 014 naissances descendantes avec brassage et 52 527 avec contact 8.
Ces nombres ne sont pas normalisés par durée et ne décident pas du verdict.

| fondateur | graine | dernières époques vivantes, brassage | contact 8 |
|---|---:|---:|---:|
| soupe 0 | 0 | 95 | 95 |
| soupe 0 | 1 | 119 | 120 |
| soupe 0 | 2 | 231 | 231 |
| soupe 0 | 3 | 159 | 159 |
| soupe 0 | 4 | 7 | 7 |
| soupe 0 | 5 | 335 | 335 |
| soupe 0 | 6 | 15 | 15 |
| soupe 0 | 7 | 71 | 63 |
| soupe 2 | 0 | 15 | 15 |
| soupe 2 | 1 | 1 | 159 |
| soupe 2 | 2 | 359 | 223 |
| soupe 2 | 3 | 15 | 15 |
| soupe 2 | 4 | 2 | 311 |
| soupe 2 | 5 | 119 | 119 |
| soupe 2 | 6 | 2 | 159 |
| soupe 2 | 7 | 295 | 295 |

### Ce que cela dit

Le contact long sauve trois démarrages précaires du fondateur soupe 2. Mais il
ralentit généralement la propagation ; la paire souvent devenue identique
n'offre plus de nouveau substrat jusqu'au brassage. Cette lecture est une
interprétation des trajectoires, pas une mesure de réparation.

Dix paires meurent exactement à la même époque. Leur sort paraît donc surtout
lié à la physique commune. Les quatre gains ne suffisent pas au signe annoncé,
et le gain moyen de 26,1 % reste très inférieur au doublement prédit.
**Laisser du temps au partenaire ne suffit pas à incuber une lignée dans cette
sonde.** Aucun mécanisme supplémentaire n'a été ajouté pour sauver l'idée.

### Définition de la mesure et limites

- Une naissance reprend le détecteur de la soupe : au moins huit octets écrits
  et changés, destination devenue identique à au moins 56/64 octets du parent
  avant interaction, alors qu'elle ne l'était pas. Ce détecteur ne modifie jamais
  les programmes. La destination reçoit l'ascendance du parent ; une copie
  issue d'un programme extérieur à la famille retire cette ascendance.
- Un porteur conserve son ascendance à travers ses mutations. Chaque époque,
  on cherche exhaustivement les porteurs encore reconnus par `u_copier` : copie
  sur trois bruits sur quatre, puis copie de la copie. La dernière époque avec
  au moins un tel porteur est la durée publiée. La première absence arrête le
  run ; on n'attribue aucune renaissance à la famille. La résolution est une
  époque, pas une instruction.
- La durée inclut le fondateur : il y a bien des départs qui meurent avant leur
  première naissance. Le témoin sans écritures vérifie seulement l'absence de
  descendance ; sa durée de conservation d'un génome potentiellement copieur
  n'est pas interprétée comme de la reproduction.
- C'est une expérience **après l'origine**, avec un copieur archivé par run,
  pas une nouvelle preuve d'émergence spontanée, de vie ouverte, ni de robustesse
  intrinsèque. Deux génotypes et une seule durée de contact sont testés.
- Le moteur d'univers existant remplace `NEURAL` par `CLRI` (écart déjà documenté
  dans universe-search). Les deux fondateurs passent les deux détecteurs avant
  les runs ; on ne prétend pas que les trajectoires sont celles de `soup_exec`.
- La durée 8 et la dérive K=8 sont fixes ; les contacts changent aux époques
  1, 9, 17… et les lois aux époques 8, 16… Aucun réglage après mesure.
- Aucun suivi des lois ni ralentissement de leur dérive n'est offert au vivant.
  La séparation des flux garantit les mêmes lois et les mêmes mutations entre
  bras jusqu'à la première extinction de l'un d'eux. Les hash finaux peuvent
  différer puisque les runs s'arrêtent à des époques différentes.

## Reproduire et vérifier

Depuis la racine du dépôt :

```console
make experiments/incubation_stepback
./experiments/incubation_stepback selftest
./experiments/incubation_stepback run > experiments/incubation-stepback/results.txt
make test
```

La cible est indépendante des labos. `src/`, `include/`, `seeds/` et
`tests/test_vm.c` sont inchangés. `make test` passe : **40/40 tests du cœur**,
dont `test_core_state_hash_matches_main` (les dix hash historiques), puis tous
les selftests des sondes. Les nouveaux contrôles vérifient les deux fondateurs,
une naissance causale, l'inertie de l'observation, le déterminisme et le témoin
sans écritures. Sortie de validation : [`validation.txt`](validation.txt).
