# Seul : une organisation qui revient, sans copie

> Simulation d'évolution dans une VM, pas une conscience ni une vie libre.

## L'approche, en une phrase (écrite avant la mesure)

Une bande de 64 octets, exécutée seule, sans autre ensemble et sans monde, peut revenir à
elle-même après qu'un de ses octets a été remplacé. Le réplicateur n'est pas le score.

## Pourquoi c'est neuf dans ce dépôt

La naissance dans la soupe est une moitié réécrite à ≥ 56/64 de l'autre moitié. `soup-replicator`,
`universe-search`, `ensembles-octet`, `dispers-octet`, `soupe-spatiale`, `monde-v0` et
`soup-terrarium` comptent cette copie, ou bien `SPLIT`. Une organisation qui reste une, se
réécrit, et n'a pas de partenaire est invisible pour ces détecteurs.

Le paramètre d'ordre métabolique et le détecteur par compression ont cherché autre chose que la
copie. Ils sont revenus négatifs dans des mondes où l'on naît encore par division.

## Dispositif

- Bande de 64 octets. Adressage relatif à B : `LOAD`/`STORE` k visent `(B + k) % 64`.
- Pas de seconde moitié. `SPLIT`, `TURN`, `STEP`, `EAT`, `ATTACK`, `EMIT` sont sans effet.
- Capteurs à 0 pendant le tirage et la marche. Un passant doit encore tenir quand chaque capteur
  lit 7, puis quand il lit le numéro du pas.
- Budget 8192 instructions, arrêt sur `YIELD` ou sortie de la bande. `LOOP` remet ip à 0.
- `src/`, `include/` et `seeds/` ne bougent pas.

## Barre, déclarée avant les chiffres

Une bande **passe** si, après une exécution de réglage :

1. une seconde exécution la laisse identique (point fixe) ;
2. cette exécution fait au moins un `STORE` (un cristal que rien n'écrit échoue) ;
3. elle porte au moins 3 valeurs d'octet distinctes ;
4. au moins **8 cases** sont réparées : pour chacune, 3 substitutions sur 4 (écarts +1, +18, +35,
   +52) ramènent la bande entière ;
5. au moins **1 case** est létale : les 4 substitutions y restent (une atteinte peut tuer ; un
   tampon qui ignore toute atteinte échoue) ;
6. les points 1, 2, 4 et 5 tiennent encore avec les capteurs à 7, puis avec les capteurs égaux au
   numéro du pas.

| bras | volume | seuil |
|---|---:|---|
| tirage uniforme | 8192 bandes | **≥ 16** passent |
| marche, mutation 0,00024 / octet / époque puis une exécution | 32 bandes × 16384 époques | **≥ 4** bandes finales passent |

Un seul seuil atteint donne **oui**. Les deux ratés donnent **non**. Un passant isolé sous le
seuil est rapporté, il ne fait pas basculer le verdict.

Contrôles, avant le tirage : le restaurateur écrit à la main passe ; le même sans `STORE` échoue ;
le cristal `0x11` échoue ; le copieur fait main de la soupe (reconnu copieur sur 128 octets) échoue
seul, il recopie l'atteinte sur elle-même ; un bruit de graine 1 échoue.

Le motif réplicateur dominant de la soupe en adressage B (`05 1d 03 02 1c 1f ff 07 01 03 09 03 c9 06 90 1e`,
quatre fois) passe seul. Sur une bande de 128 il écrit vers +64, l'autre moitié. Sur 64, l'immédiat
`0x90` retombe 16 cases plus loin, sur la période suivante, et le motif se répare. Ce contrôle ne
change pas les seuils du tirage ni de la marche. Il dit que le détecteur voit cette organisation
quand le partenaire n'est plus là.

## Reproduire

```
make experiments/seul
./experiments/seul selftest
./experiments/seul > experiments/seul-duree/results.txt
```
