# Résultat : oui à la diversification dans ce modèle

Mesure exécutée **après** le pré-enregistrement `f0e7651`. Le dispositif, le
budget et la barre n'ont pas changé après les chiffres. Le code est
`pari.c`, autonome en C11, libc/libm ; aucun code du moteur n'est inclus.

**Verdict pré-enregistré : OUI, 31 courses sur 32 passent, pour 24 exigées.**
Il s'agit d'évolution d'un caractère dans un modèle qui offre la reproduction,
pas d'une naissance spontanée de réplicateur. Les deux octets mutés et transmis
finissent entre 5 et 11, soit des probabilités par pas de 1,95 à 4,30 % ; la
fréquence des changements de milieu est de 3,125 %.

| Condition de passage (validation réservée) | Courses qui la tiennent | Seuil par course |
|---|---:|---|
| Deux octets strictement entre 0 et 255 | 32 / 32 | oui |
| Sélection − bruit neutre | 31 / 32 | ≥ 0,10 log-unité/pas |
| Sélection − meilleur des deux cristaux | 32 / 32 | ≥ 0,10 log-unité/pas |
| Sélection − même génome en bascule synchrone | 32 / 32 | ≥ 0,10 log-unité/pas |
| Les quatre conditions simultanées | **31 / 32** | **≥ 24 / 32** |

| Bras | Moyenne log-croissance par pas (32 courses) |
|---|---:|
| Sélection et diversification | +0,271862574 |
| Bruit neutre, budget identique | −0,535091102 |
| Meilleur phénotype constant, choisi après validation | −1,264839657 |
| Même génome sélectionné, changement synchrone | −1,295814481 |

La graine 17 échoue : son témoin neutre tire les octets `(22,32)` et donne
+0,229071893, contre +0,271524692 pour `(7,8)` sélectionné. L'écart de
+0,042452798 est inférieur à la marge de 0,10. Ce témoin aléatoire est déjà
une stratégie viable ; il n'est pas renommé en succès.

## Les 32 comptes, sans sélection a posteriori

`a,b` sont les deux octets finaux du bras sélectionné ; `n_a,n_b` ceux du
témoin neutre. Les quatre scores sont des moyennes de 16 milieux réservés de
4096 pas. `Passe` applique toutes les conditions sur les valeurs non arrondies.

| Graine | a,b | n_a,n_b | Sélection | Bruit | Cristal | Synchrone | Δ bruit | Passe |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 7,7 | 251,209 | 0.273487 | -1.080125 | -1.220867 | -1.295134 | 1.353612 | oui |
| 1 | 8,7 | 213,255 | 0.272372 | -1.095747 | -1.246349 | -1.298329 | 1.368119 | oui |
| 2 | 7,5 | 172,156 | 0.269961 | -0.513041 | -1.245363 | -1.303295 | 0.783002 | oui |
| 3 | 8,6 | 3,222 | 0.276643 | -0.609255 | -1.263839 | -1.299409 | 0.885897 | oui |
| 4 | 8,7 | 64,74 | 0.273778 | 0.066335 | -1.273752 | -1.293625 | 0.207443 | oui |
| 5 | 8,7 | 241,2 | 0.272883 | -0.961969 | -1.282159 | -1.294387 | 1.234852 | oui |
| 6 | 8,8 | 189,129 | 0.274306 | -0.497971 | -1.264825 | -1.295134 | 0.772277 | oui |
| 7 | 8,6 | 113,247 | 0.273582 | -0.802080 | -1.284702 | -1.296330 | 1.075662 | oui |
| 8 | 9,8 | 44,185 | 0.273631 | -0.315057 | -1.271260 | -1.293761 | 0.588688 | oui |
| 9 | 8,8 | 79,77 | 0.272370 | 0.024467 | -1.275412 | -1.295134 | 0.247903 | oui |
| 10 | 7,9 | 248,181 | 0.269606 | -0.964574 | -1.280862 | -1.293423 | 1.234180 | oui |
| 11 | 7,7 | 133,66 | 0.273107 | -0.112478 | -1.275516 | -1.295134 | 0.385585 | oui |
| 12 | 6,6 | 138,42 | 0.270461 | -0.082987 | -1.292591 | -1.295134 | 0.353448 | oui |
| 13 | 7,5 | 208,72 | 0.267354 | -0.495160 | -1.283664 | -1.296801 | 0.762513 | oui |
| 14 | 8,8 | 236,24 | 0.274291 | -0.710992 | -1.262697 | -1.295134 | 0.985283 | oui |
| 15 | 11,7 | 9,0 | 0.271898 | -1.123110 | -1.282159 | -1.298070 | 1.395008 | oui |
| 16 | 8,7 | 9,203 | 0.272578 | -0.443420 | -1.227043 | -1.290679 | 0.715998 | oui |
| 17 | 7,8 | 22,32 | 0.271525 | 0.229072 | -1.259324 | -1.297545 | 0.042453 | non |
| 18 | 7,7 | 62,42 | 0.267280 | 0.133518 | -1.197409 | -1.295134 | 0.133761 | oui |
| 19 | 9,9 | 246,18 | 0.272263 | -0.859958 | -1.276606 | -1.295134 | 1.132221 | oui |
| 20 | 6,8 | 87,231 | 0.268662 | -0.691945 | -1.239395 | -1.303251 | 0.960607 | oui |
| 21 | 10,8 | 179,0 | 0.273072 | -1.293260 | -1.269807 | -1.297834 | 1.566332 | oui |
| 22 | 8,8 | 53,73 | 0.272461 | 0.092538 | -1.269600 | -1.295134 | 0.179923 | oui |
| 23 | 6,8 | 31,137 | 0.268353 | -0.052427 | -1.238824 | -1.287257 | 0.320780 | oui |
| 24 | 7,6 | 237,68 | 0.273587 | -0.704133 | -1.289892 | -1.295487 | 0.977720 | oui |
| 25 | 7,6 | 234,223 | 0.271251 | -1.049446 | -1.275049 | -1.296620 | 1.320697 | oui |
| 26 | 6,7 | 191,156 | 0.272141 | -0.592051 | -1.266486 | -1.297284 | 0.864192 | oui |
| 27 | 9,8 | 185,233 | 0.273632 | -0.896036 | -1.261763 | -1.297109 | 1.169668 | oui |
| 28 | 8,6 | 58,142 | 0.271748 | -0.123637 | -1.271364 | -1.298717 | 0.395385 | oui |
| 29 | 8,10 | 241,76 | 0.269957 | -0.744938 | -1.272039 | -1.292706 | 1.014895 | oui |
| 30 | 6,5 | 183,154 | 0.269702 | -0.549320 | -1.270586 | -1.292809 | 0.819023 | oui |
| 31 | 8,8 | 12,188 | 0.271663 | -0.303731 | -1.283664 | -1.295134 | 0.575394 | oui |

## Vérification et preuve

Compilation stricte : `-O2 -std=c11 -Wall -Wextra -Werror -pedantic`.
`selftest` vert : conservation, calcul analytique sur un pas, témoins
cristal/synchrone, déterminisme des milieux et de deux évolutions réduites.
Le même selftest est vert sous AddressSanitizer et UndefinedBehaviorSanitizer.
Aucun sanitizer n'a signalé d'erreur. Le seul problème de compilation initial
était un qualificateur de tableau incompatible avec C11 strict ; corrigé avant
le premier run complet par un passage de tableau aplati.

Preuves brutes : `/tmp/pari-0996a2cd-mesure-a01/`, avec `selftest.txt`,
`resultats.txt`, `SHA256SUMS` et l'ELF neuf `pari`. Le script `run.sh` refuse un
chemin déjà existant. Empreintes SHA-256 du run :

```text
pari.c        813f3cfe25b0af5a3d1064ce2f9852005e596d7ac4ce52d8f010c76094745600
README.md     a165858e8019a33739b9d7e8f5be2f33fb8b82a4dbee5f9866abac4c0f1407df
ELF pari      500ea711578eba64c2a306d7b0e13bc9ec25287a7139953d0b6d1b5325ae62ba
selftest.txt  8643aac5b42dd32f7a315e84f6c43778793b16f5d4adf55dccf8a41075ef082c
resultats.txt ffef72bdd4ef2229857acf1d26905cc650d7c369242435a0b09df8eca5cab578
```

Le témoin synchrone sépare diversification simultanée et simple changement
aléatoire du clone ; le témoin neutre sépare cette propriété offerte par le
modèle de son réglage par sélection. La conclusion reste limitée aux deux
états, à la colonie de taille infinie et aux risques fixés dans le protocole.
La sélection règle ici deux probabilités ; aucune ouverture indéfinie de
l'évolution ni généralisation à l'ISA historique n'est mesurée.
