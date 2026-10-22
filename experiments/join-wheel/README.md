# Joint : naissance, lois héritées, ombre

Trois pièces déjà mesurées, jamais dans le même run.

1. Un réplicateur naît du bruit (soupe, adressage B, `big_b_0` archivé).
2. Chaque individu porte sa table, la transmet quand il réécrit l'autre moitié (≥ L/2), et dérive seul (débit de `local-sans-W`, K=8). Pas de `WRITE_LAW`.
3. L'ombre reçoit les mêmes coups de dérive, aux mêmes places. À chaque héritage réel, elle copie la table d'un individu tiré au hasard, pas celle de l'auteur.

Une table nouvelle est persistante quand elle a au moins 3 porteurs pendant 4 échantillons de suite (un échantillon toutes les 64 époques). La table d'origine ne compte pas.

## Pré-enregistrement

Avant la mesure. N=512, 1024 époques, graines 0–3, une copie du fondateur dans le bruit. Bras bruit : graine 0, 512 époques, personne.

- Copieurs en fin (≥ 0,25 de 64 tirages) sur **au moins 3 graines sur 4**.
- Espèces nouvelles persistantes : réel **strictement au-dessus** de l'ombre sur **au moins 3 graines sur 4**.
- Distance moyenne à la table d'origine : réel **plus bas** que l'ombre sur **au moins 3 graines sur 4**.
- Bras bruit : **0 copieur** à la fin.

Un seul seuil raté réfute le joint à cette échelle. Deux zéros de nouveauté alors que les copieurs vivent : non concluant, horizon trop court. Pas de second mécanisme après coup.

## Résultats — dérive dès la première époque

Mesuré. N=512, 1024 époques, dérive et héritage dès l'époque 1. Courbe = copieurs sur 16 tirages, toutes les 64 époques.

| graine | prise | copieurs fin | espèces réel | ombre | dist réel | dist ombre | naissances |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 64 | 0/64 | 0 | 0 | 88,45 | 89,55 | 8711 |
| 1 | 64 | 0/64 | 0 | 0 | 94,91 | 89,50 | 2700 |
| 2 | — | 0/64 | 0 | 0 | 90,25 | 89,41 | 36 |
| 3 | 64 | 0/64 | 0 | 0 | 91,66 | 87,31 | 78384 |
| bruit | — | 0/64 | 0 | 0 | 49,34 | 49,68 | 0 |

**Verdict : non.** Les trois seuils sont ratés. La lignée apparaît (graines 0, 1 et 3 prises à l'époque 64) puis plus aucun copieur vérifié en fin. Aucune table nouvelle ne tient 4 échantillons. La distance réelle n'est pas plus basse que l'ombre (1 graine sur 4). Le bruit ne naît pas.

## Pré-enregistrement — après la prise de contrôle

La question posée était le joint **au-delà** de la prise. Ce bras-ci : 256 époques de physique d'origine figée (une seule table, pas de dérive), puis 1024 époques de lois locales héritées et d'ombre, mêmes seuils.

- Copieurs en fin sur **au moins 3 graines sur 4**.
- Espèces nouvelles : réel strictement au-dessus de l'ombre sur **au moins 3 graines sur 4**.
- Distance à l'origine : réel plus bas que l'ombre sur **au moins 3 graines sur 4**.
- Bruit, même calendrier : **0 copieur** en fin.

Écrit avant ce run. Pas de troisième bras si celui-ci rate.

## Résultats — après la prise

256 époques figées, puis 1024 de dérive. `cop_est` est le sondage à l'époque 256 (sur 16). La courbe compte les copieurs sur 16, toutes les 64 époques.

| graine | à la prise | copieurs fin | espèces réel | ombre | dist réel | dist ombre | tables distinctes réel / ombre |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 15/16 | **26/64** | 0 | 0 | 90,73 | 91,56 | 237 / 251 |
| 1 | 16/16 | 0/64 | 0 | 0 | 89,52 | 89,50 | 512 / 512 |
| 2 | 0/16 | 0/64 | 0 | 0 | 93,75 | 89,43 | 501 / 504 |
| 3 | 16/16 | 0/64 | 0 | 0 | 88,37 | 87,80 | 507 / 509 |
| bruit | 0/16 | 0/64 | 0 | 0 | 49,82 | 49,82 | 512 / 512 |

Graine 0, copieurs sur 16 le long du run : 15, 15, 15, 15, puis 12, 11, 0, 0, 0, 0, 5, 14, 14, 11, 0, 13, 11, 10, 7, 7. Elle s'éteint et revient. Graine 3 tient longtemps (jusqu'à 16/16 en pleine dérive) puis tombe à 0 avant la fin.

**Verdict : non.** Copieurs en fin : 1 graine sur 4. Distance : 1 graine sur 4. Espèces : 0 des deux côtés, sur les cinq runs.

Le compteur d'espèces ne pouvait pas sonner. Chaque individu est touché environ une fois toutes les 9 époques. Sur 64 époques, presque toutes les tables changent. Aucune table ne reste identique pendant 4 échantillons. Le zéro n'est pas une course entre réel et ombre : le critère d'identité est hors d'atteinte à ce débit. Ce qui parle, c'est la lignée. Elle prend le contrôle, puis la dérive locale l'éteint, avec un retour sur une graine.

Le bruit reste à 0 naissance et 0 copieur. Sorties : `results.txt`, `results-est.txt`.
