# Autopsie du survivant de la dérive K=8 (v2, partie 3)

Question de Dani : en v2, une seule soupe sur 32 a survécu à la dérive lente de la physique au rythme K=8
(un changement de loi toutes les 8 époques, soit 512 changements). Qu'est-ce qui l'a sauvée ?

**Réponse courte : rien ne l'a sauvée. Elle est morte, puis la vie est réapparue.** La lignée injectée s'est éteinte
(0 copieur vérifié sur les 1024 programmes à 9 points de contrôle entre les époques 1664 et 2944). Ensuite la vie est
revenue dans une physique d'arrivée qui se trouvait être fertile. Le « survivant » est une **résurrection** (hypothèse e).
La chance du trajet (a) y contribue, mais il n'y a ni robustesse intrinsèque (b) ni adaptation active (c), et aucune
diversité ne l'a sauvée (d).

## Outil

`experiments/k8_survivor.c` inclut `universe_search2.c` (sans son `main` : garde `US2_NO_MAIN`, +2 lignes,
comportement inchangé). Il est câblé dans le `Makefile` (`make experiments/k8_survivor`, selftest dans `make test`).
`trace_run` est une copie de `drift_run` (mêmes tirages, dans le même ordre) à laquelle j'ai ajouté des mesures :
- le journal des changements de loi ;
- des statistiques sur la population entière à chaque point de contrôle (64 époques) ;
- un knockout optionnel.

Le selftest vérifie que `trace_run` rejoue `drift_run` octet pour octet.

Les finalistes de v2 ne sont pas versionnés. On les régénère de façon déterministe :

    ./experiments/universe_search2 race --seed 2 --save /tmp/k8_fin2.bin            # ~4 min, sortie identique à v2/race_seed2.txt (hors chronos)
    ./experiments/k8_survivor replay   --load /tmp/k8_fin2.bin                       # 32 dérives + 32 sauts
    ./experiments/k8_survivor trace    --load /tmp/k8_fin2.bin --src 0 --rep 5 [--mode 1]
    ./experiments/k8_survivor robust   --load /tmp/k8_fin2.bin                       # (b)
    ./experiments/k8_survivor fresh    --load /tmp/k8_fin2.bin --seeds 16            # (e) bruit pur
    ./experiments/k8_survivor fertile  --load /tmp/k8_fin2.bin --seeds 8             # (e) fertilité des 32 physiques finales
    ./experiments/k8_survivor knockout --load /tmp/k8_fin2.bin --src 0 --rep 5 --at 2944 --h 16

Les sorties brutes sont dans ce dossier : `replay.txt`, `trace_*.txt`, `robust.txt`, `fresh.txt`, `fertile.txt`,
`knockout.txt`. Calcul total : environ 13 min sur 8 fils.

## 1. Identité et rejeu

Le survivant est la **source S#0 de l'arm sélection, répétition 5**, graine `mix(7, 5)`. S#0 a les paramètres suivants :
L=16, kper=4, adressage absolu, rcost=4, wcost=2, steps=1024, mut=0,001. Le réplicateur injecté est
`350000ae00f100000000f100ae000035`.

Le rejeu est exact :
- dérive 1/32 et saut 3/32, comme publié en v2 ;
- distances de table moyennes 351,5 / 359,5 / 205,0 / 295,8, comme publié ;
- `trace_run == drift_run` pour les quatre runs survivants.

Les survivants du saut sont **S#0 rép 5** (même physique d'arrivée que le survivant de la dérive), **T#0 rép 6** et
**T#0 rép 7**.

## 2. Trajectoire de S#0 rép 5 (dérive)

| époque | dérive | copieurs vérifiés / 1024 | dominant | ce qui se passe |
|---|---|---|---|---|
| 512 | 0 | 877 | injecté | établi |
| 832 | 320 | 769 | injecté | 34 lois changées, aucune ne touche un octet utilisé |
| 896–1408 | 384–896 | 646 → 95 | injecté puis variant à H=6 | érosion sans aucune casse directe : elle coïncide avec mut 0,001→0,004 (dé 376), steps 1024→2048 et rcost 4→1→2 |
| **1664–1728** | 1152–1216 | **0** | — | **extinction** |
| 1792–2176 | | 0 à 67 | variants épars `35000000…353500…` | renaissances fugaces |
| **2240, 2496–2816, 2944** | | **0** (≤2 entre-temps) | — | **mort à nouveau** |
| 3008–3072 | 2496–2560 | 31 → 294 | `e2000000f12df1c6c6f12df1000000e2` | redémarrage |
| 3776 → 4608 | → 4096 | 320 → 633 | `35000000f18652c6c63b86f100000035` (et son jumeau `…863bc6c652…`) | dominant final |

Pendant toute la dérive, seuls **2 changements sur 512** ont touché un octet utilisé par le dominant du moment, et
**aucun** n'a cassé ce dominant. Le seul coup fatal pour l'injecté est `tab[3][0x35] LOOP→STORE` (dé 1584). Il est
arrivé après la première extinction.

Au point le plus bas, la soupe ne contient plus que 16 à 70 génotypes distincts : c'est une quasi-monoculture de
reliques non copieuses.

## 3. Hypothèses et contrôles

**(a) Chance du trajet : oui, mais elle n'a pas suffi.**
- Sur les 10 entrées (q, octet) essentielles de l'injecté, seulement **1 est réassignée** en fin de trajet.
- Les 7 autres répétitions de S#0 en ont 3 à 6 (moyenne 4,1).
- L'attendu est d'environ 3,5 (459 changements de table sur 1024 entrées), et P(≤1) ≈ 0,09.
- Malgré ce trajet clément, la lignée injectée s'éteint quand même.

**(b) Robustesse intrinsèque : non.**
- Avec 352 entrées re-tirées au hasard (distance de la vraie dérive), l'injecté S#0 reste copieur dans **6,0 %** des
  400 tables testées. L'attendu est de 3,6 % si toucher une entrée essentielle tue.
- Le dominant final fait mieux, **13,5 %**, mais reste peu robuste.
- La réparation en une mutation (« +1mut ») ne relève l'injecté qu'à 12 %.
- Rien ne distingue S#0 des autres sources : S#1 est à 10,7 %.

**(c) Adaptation active : non.**
- L'injecté copie sous la physique de base mais **ne copie plus sous la physique finale**.
- Le dominant final copie sous la physique finale **et aussi sous celle de base**.
- 11 de ses 12 lois essentielles existaient déjà au départ. Une seule est née de la dérive : `tab[3][35]=STORE`, et le
  génome copie même quand cette entrée vaut LOOP.
- Ce n'est donc pas un génome qui aurait suivi la physique pas à pas. C'est une autre solution de la même famille.

**(d) Redondance ou diversité : non.**
- Au creux, il y a 0 à 5 génotypes copieurs et 16 à 70 génotypes au total.
- En fin de run, il reste 2 à 6 génotypes copieurs.
- Aucun réservoir de variants n'a porté la lignée.

**(e) Mort puis réapparition : oui.**
- **Rupture de lignée.** On compte 0 copieur vérifié dans la population entière à 9 points de contrôle. Le dominant
  final est à H=10 de l'injecté. Aucun programme n'est à H≤2 du dominant final avant l'époque 3136.
- **Les reliques ne sont pas nécessaires.** On efface toute la soupe (remplacée par du bruit) à l'époque 1664 ou à
  l'époque 2944. Dans les deux cas, la vie revient seule et la soupe finit vivante (cf 0,97 et 0,91, contre 0,77 sans
  knockout ; 2/2). Les knockouts partiels (H≤4 : vivant ; H≤8 : mort) montrent surtout que chaque trajectoire est un
  tirage stochastique (n=1).
- **La physique d'arrivée est fertile.** Je lance une soupe de bruit pur, sans injection, sous la physique finale de
  chacune des 32 dérives (8 graines chacune). La fin de S#0 rép 5 fait émerger la vie dans **3/8** cas. Les 7 autres
  fins de S#0 donnent **0/56** (Fisher p = 0,0013). Avec 16 graines fraîches, on obtient 2/16. La physique de base de
  S#0 est très fertile (13/16), et la dérive détruit cette fertilité dans 7 répétitions sur 8.
- **Toutes les survies sont des émergences en terre fertile.** Les 4 événements de survie (1 dérive + 3 sauts) tombent
  tous dans les 8 physiques finales fertiles (≥1/8) : 4 runs vivants sur 16. Les 24 physiques stériles donnent 0 sur
  48 (Fisher p = 0,003).
- **Le saut S#0 rép 5 confirme le mécanisme.** L'injecté y meurt dès l'époque 576, ses reliques s'effacent, puis
  `e2000000000000e2e2000000000000e2` émerge vers l'époque 1344. C'est la même famille de copieur inverse quasi
  palindromique (H1_DEC aux deux bouts). On la retrouve aussi dans la physique de base partant du bruit
  (`e20000000000f20000f20000000000e2`).
- **Les survivants T#0 du saut sont de la vie neuve.** T#0 rép 6 reste mort de l'époque 576 à 4096 et ne renaît
  qu'à l'époque 4160. T#0 rép 7 renaît à l'époque 832. Leurs dominants sont à H≈53/64 de l'injecté et utilisent des
  lois neuves (4/6 et 11/14), que la base ne permet pas.

## 4. Verdict

**(e), aidé par (a).** La survie K=8 n'est pas une survie : c'est une **ré-émergence spontanée** dans une physique
d'arrivée fertile. Le trajet de dérive a été clément (1/10 entrée essentielle réassignée, P≈0,09). Il ne s'agit ni de
robustesse intrinsèque (6 %), ni d'adaptation (le dominant final copie aussi sous la base), ni de diversité.

**Correction pour v2.** Le critère « vivant en fin de run » compte des résurrections. Avec un critère de
**continuité de lignée** (jamais 0 copieur dans la population entière), la dérive K=8 donne **0/32**, et les 3
survivants du saut sont eux aussi des renaissances.

## 5. Expérience suivante proposée

On ne peut pas reproduire délibérément une robustesse qui n'existe pas. Il faut donc d'abord mesurer la bonne chose,
puis sélectionner pour elle.
1. **Métrique de continuité.** Ajouter à `drift_run` la notion de lignée continue (copieurs > 0 dans la population
   entière à chaque point de contrôle) et refaire le balayage K = 8, 16, 32, 64, 128. On cherche le seuil de K où
   une vraie continuité apparaît.
2. **Sélection pour l'évolvabilité sous dérive.** Course (racing) sur univers × réplicateur, où la fitness est le
   temps de lignée continue sous une dérive K=8 aléatoire (plusieurs trajectoires par candidat). Comparer à un témoin
   aléatoire, avec les mêmes contrôles que ci-dessus :
   - le dominant final copie-t-il encore sous la base ?
   - quelle part de ses lois essentielles est née de la dérive ?
   - la survie résiste-t-elle à un knockout des reliques ?

   Une vraie adaptation se reconnaîtrait à un dominant final qui ne copie plus sous la base mais dont la lignée n'a
   jamais été interrompue.
3. **Variante peu coûteuse.** Sélectionner des physiques pour leur fertilité **robuste à la dérive** : une émergence
   depuis le bruit qui persiste après n changements aléatoires de lois. C'est ce qui a réellement « sauvé » le cas
   présent.
