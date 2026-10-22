# Un octet de la lignée 3 débloque la lignée 2

Mesure du 22 octobre 2026, moteur de `main` **382747b2507f8ab4219c3207c8cdaef1bc2ac5df**.
Les checkpoints annoncés n'étaient pas présents dans l'environnement d'exécution.
Les trois courses ont été refaites, sans changer le moteur :

```console
make
./octet-adn --labs general --cycles 60 --seed 0 --no-reload --data-dir /tmp/oadn-go2
./octet-adn --labs general --cycles 60 --seed 3 --no-reload --data-dir /tmp/oadn-go-s3
./octet-adn --labs general --cycles 60 --seed 2 --no-reload --data-dir /tmp/oadn-go-s2
```

200 ticks/cycle (défaut), sélection appariée, delay_rate 0,12 déjà présent.
Les fichiers `.adn` voisins sont les génomes de naissance extraits des pools au cycle
60 : indices **12 / 3 / 3** pour les graines **0 / 3 / 2**. Ce ne sont ni les champions
affichés, ni le soma. Les checkpoints et les courses restent hors dépôt.

Les deux donneurs tiennent mémoire-2 = 1,000. Ils diffèrent à **20 positions**, listées
avec leurs valeurs dans [results.txt](results.txt). Les cinq substitutions communes
déjà connues sont présentes dans les trois génomes ; elles ne sont pas une découverte.

Sur la base graine 2, on essaie tous les allèles d'un octet provenant de l'un ou
l'autre donneur, y compris les retours à la valeur de la seed. Deux allèles distincts
au même emplacement sont essayés séparément ; un allèle commun n'est essayé qu'une
fois. **36 transferts**, tous évalués par `gn_check`, soit 40 tâches tenues à l'écart
par mémoire et par condition, sans changer le critère du binaire.

Un seul transfert passe : **0x79 : 17 → cf**, provenant de la graine 3. L'ensemble
vide ne passe pas. Un octet est donc un minimum global en nombre de substitutions
sur cette base ; l'unicité vaut parmi les 36 transferts essayés. Ce protocole ne
prétend pas établir le minimum sur la seed ni chercher tous les octets possibles.

| Génome | m2 | m3 | Gelé m2/m3 | Sans récompense m2/m3 | known | Distance seed | Longueur | Retour binaire seul |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Seed | 0,366406 | 0,400781 | 0,287500 / 0,292969 | 0,266406 / 0,255469 | 0,725781 | 0 | 172 | 3 |
| Graine 0, pool 12 | 1,000000 | 0,358594 | 0,188281 / 0,191406 | 0,251563 / 0,237500 | 0,726563 | 15 | 172 | 0 |
| Graine 3, pool 3 | 1,000000 | 0,354688 | 0,188281 / 0,191406 | 0,239844 / 0,261719 | 0,725781 | 16 | 172 | 0 |
| Graine 2, pool 3 | 0,529688 | 0,360156 | 0,299219 / 0,293750 | 0,266406 / 0,255469 | 0,678125 | 20 | 172 | 3 |
| Graine 2 + 0x79=cf | **0,986719** | 0,353906 | **0,260156** / 0,251563 | **0,266406** / 0,255469 | **0,798438** | 21 | 172 | **0** |

La chute mémoire-2 est de **0,726563** gelé et **0,720313** sans récompense.
Le gain sur la base est de **0,457031** en mémoire-2 et **0,120313** en known.
La mémoire-3 baisse de 0,006250. Aucune émergence mémoire-3.

Interprétation du code : la graine 3 enlève l'ADD qui mélange l'indice courant avec
la case 19. L'intron `cf` est déjà traité par la VM, sans nouvel opcode. La graine 0
annule plutôt l'indice courant avec `AND 0` à 0x6d. Cet allèle de la graine 0, seul
sur la même base graine 2, donne seulement mémoire-2 **0,420313**. Les solutions des
deux donneurs ne sont donc pas interchangeables dans ce contexte génétique.

## Reproduire l'ablation et le verdict

Depuis la racine du dépôt :

```console
make experiments/general_transfer
./experiments/general_transfer /tmp/general-transfer.adn > /tmp/general-transfer.txt
diff -u experiments/general-transfer/results.txt /tmp/general-transfer.txt
./octet-adn general /tmp/general-transfer.adn
echo $?
```

Résultat du dernier appel, effectué séparément :

```text
/tmp/general-transfer.adn[0] 172 o, 21 o ≠ seed  m2=0.987 m3=0.354  gelé 0.260/0.252  sans_récompense 0.266/0.255  known=0.798 train=0.579  ÉMERGENCE mémoire-2
émergence : oui
0
```

Les codes de la table ont aussi été vérifiés avec un appel séparé du binaire pour
chaque entrée. Le `code` de `results.txt` est dérivé du même `gn_check`.

Arrêt au premier fait nouveau combinant deux lignées, conformément à la consigne :
pas de nouvel opérateur de naissance, de course avec cet opérateur, ni de recherche
mémoire-3. Le transfert est une intervention expérimentale, pas une nouvelle
apparition spontanée dans une course. Aucun fichier `seeds/` ni opcode n'a changé.

Validation : `make test`, **35/35**, dont `test_core_state_hash_matches_main` pour
les dix labos historiques. SHA-256 inchangé de `seeds/general.adn` :
`b61b2cef91847ce13ee867c4a991acc477a4ac5133fd684e0b92a3f11192d199`.
