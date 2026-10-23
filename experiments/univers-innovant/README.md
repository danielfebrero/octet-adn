# Univers innovants : un ensemble peut-il naître d'une loi locale, sans génome

> Simulation d'un milieu, pas une conscience ni une vie libre.

## L'approche, en une phrase (écrite avant la mesure)

Huit familles de lois locales, figées, sans pointeur et sans `STORE`, font évoluer un
anneau de 64 octets. Un ensemble émerge si le bruit de plusieurs graines tombe dans le
même motif, et si un cristal `0x11` ne peint pas ce motif.

## Pourquoi c'est neuf dans ce dépôt

`soup-replicator`, `universe-search` et ses suites, `ensembles-octet`, `dispers-octet`,
`soupe-spatiale`, `monde-v0`, `seul` et `motif-artefact` exécutent l'ISA. Le score y est
une copie, une réparation de bande, ou une table de cette même machine. Ici il n'y a pas
de génome, pas de naissance par réécriture d'une moitié, pas de `u_copier`. La loi est
un voisinage. L'ensemble est lu sur l'état.

## Dispositif

Anneau de 64. Mise à jour synchrone. Chaque case lit la gauche, elle-même, la droite.
`src/`, `include/` et `seeds/` ne bougent pas. Ce programme ne les inclut pas.

Générateur `xorshift` démarré à 1. Chaque loi consomme 4 octets, dans l'ordre des
familles puis des tirages. Huit graines de bruit, graines `1000 + s` pour `s` de 1 à 8.
Brûlure de 128 pas, puis la plus petite période temporelle `τ` dans 1..16. Sans `τ`,
la loi n'a pas d'ensemble sur cette graine.

| famille | règle |
|---|---|
| affine | `(p0·gauche + p1·soi + p2·droite + p3) mod 256` |
| médiane | médiane des trois, puis `xor p0` |
| seuil | si la somme dépasse `p0`, `soi + p1`, sinon `soi − p2` |
| cyclique | module `M = 3 + (p0 mod 14)` ; avance si un voisin est déjà l'état suivant |
| excitable | module `M = 3 + (p0 mod 6)` ; le repos s'allume contre un excité, le reste avance |
| nibble | `(soi & p0) \| ((gauche xor droite xor p2) & p1)` |
| accord | si gauche = droite, prendre cette valeur, sinon `soi + p0` |
| onde | `(gauche + droite − soi + p0) mod 256` |

## Barre, déclarée avant les chiffres

Une graine **qualifie** si, sur l'état réglé :

1. `τ` existe dans 1..16 ;
2. le nombre de valeurs distinctes est entre 3 et 16 ;
3. l'octet majoritaire occupe au plus les trois quarts de l'anneau ;
4. la plus petite période spatiale `P` dans 2..32 a au moins la moitié des cases égales
   à la case `P` plus loin ;
5. le motif majoritaire de cette période a au moins 3 valeurs, et le recouvrir sur
   l'anneau couvre au moins la moitié des cases.

Le motif canonique est la plus petite écriture hexadécimale parmi les rotations et
l'image miroir. Deux graines partagent un ensemble quand elles ont le même `P`, le même
`τ` et le même canonique.

Une loi **passe** si au moins 4 graines sur 8 partagent un ensemble, et si le cristal
`0x11`, sous la même loi, ne qualifie pas vers ce même canonique.

| bras | volume | seuil |
|---|---:|---|
| huit familles | 32 lois chacune, 8 graines | **≥ 4 lois** passent |

Zéro à trois lois : **non**. Le journal liste toute loi dont au moins 2 graines
partagent un motif. Ce journal ne crée pas un second seuil.

Étalons, sinon le dispositif est invalide :

- l'identité sur le motif planté `01 02 04 08 10 20 40 80` répété qualifie ;
- l'identité sur les 8 bruits ne qualifie aucune graine ;
- l'identité sur un anneau `0x11` ne qualifie pas.

## Reproduire

```
make experiments/univers_innovant
./experiments/univers_innovant > experiments/univers-innovant/results.txt
```

## Résultats

Dispositif valide. Sortie brute : `results.txt`.

| étalon | lecture |
|---|---|
| motif planté, identité | vu |
| bruit gelé, identité | 0 / 8 |
| cristal `0x11`, identité | refusé |

| bras | lois qui passent | meilleur bassin |
|---|---:|---:|
| 8 familles × 32 lois × 8 graines | 0 | 0 / 8 |

Le journal des lois à 2 graines est vide.

**Verdict : non.** À ce volume, ces huit familles ne font pas naître un ensemble partagé.