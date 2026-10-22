# Gardiens des lois : la vie peut-elle protéger ses lois de façon ÉMERGENTE ? (v2, partie 5)

Question de Dani : « la vie qui protège ses lois, mais cela doit être ÉMERGENT ». Pas d'oracle : la machine ne sait
pas quelles lois sont essentielles, et rien ne récompense une réparation.

**Réponse courte : non.** Quand on donne à la vie un accès direct à ses lois (bras A), elle ne les protège pas. Elle
écrit surtout là où pointent ses propres octets (un biais mécanique) et elle casse son dominant presque 4 fois plus
souvent qu'elle ne le répare (30 contre 8). Elle vit moitié moins longtemps que sans cet accès (part vivante 0.079
contre 0.150, p=0.006). Elle ne fait pas mieux que des écritures envoyées au hasard (0.101, p=0.50). Le seul
« réparateur » trouvé n'est pas un gardien : l'ablation de ses écritures *améliore* la survie (0.386 contre 0.138).

À l'inverse, une inertie générique par l'usage (bras B) protège fortement :
- part vivante 0.476 contre 0.150 (p<5e-5) ;
- 0.476 contre 0.227 pour le même mécanisme découplé de l'usage (p=1e-4) ;
- univers encore fertiles en fin : 8/32 contre 1/32.

Mais B est un mécanisme posé dans la physique, pas une protection émergente. Personne ne survit strictement à
1024 pas de dérive, dans aucun bras (0/32 partout).

## Design (pourquoi ainsi)

`experiments/law_keepers.c` inclut `rebirth.c` sans son `main` (garde `RB_NO_MAIN`, +2 lignes). Il a sa propre
VM, `lx_exec`, qui est `u_exec` plus deux primitives. Le selftest vérifie que, sans ces deux primitives, `lx_exec`
reproduit `u_exec` octet pour octet. Il fait partie de `make test`.

**Bras A, les lois dans le monde.** La table des lois (kper×256 octets) est un ruban partagé par toute la soupe :
- `READ_LAW` : a = loi[phase de l'instruction][b] ;
- `WRITE_LAW` : loi[phase][b] = a mod 30. Il écrit 1 octet de loi et 0 octet de bande. L'anti-triche est vérifiée
  instruction par instruction : jamais plus d'un octet écrit, bande et lois confondues.

Les deux primitives sont dans la table de départ à leur densité naturelle (1/30 chacune). Elles peuvent dériver
comme les autres. La dérive est celle de v2 (K=8, 90 % une entrée, 10 % un paramètre), avec un catalogue de 30
valeurs commun à tous les bras.

**Inertie des lois : un jeton de réécriture par pas de dérive.** Le jeton est pris par la première `WRITE_LAW`
exécutée et ne se cumule pas. La vie écrit donc au même débit que la dérive. La question devient « vise-t-elle
juste ? » et non « qui écrit le plus ». Sans ce plafond, des milliers d'écritures aveugles par époque
(2196 tentatives/époque mesurées) détruiraient la physique trivialement.

Les copies de vérification (copieur, comptage, trace) n'écrivent jamais dans les lois.

**Bras B, l'inertie par l'usage.** Chaque entrée de la table a un usage récent : ses exécutions dans la soupe, en
moyenne glissante sur environ 64 époques. Quand la dérive frappe, elle choisit l'entrée avec un poids 1/(1+f), où
f = usage/usage moyen. Le débit de dérive est inchangé. B ne sait pas qui exécute une entrée ni pourquoi, mais c'est
une loi de la physique. Je le présente donc comme un mécanisme générique, pas comme de l'émergence.

**Témoins :**

| Témoin | Ce qu'il fait | Rôle |
|---|---|---|
| v2 | primitives de loi inertes | dérive pure |
| A-jet | `WRITE_LAW` sans effet | `READ_LAW` marche, même coût en pas |
| A-redir | même débit d'écritures, mais position tirée au hasard | élimine le ciblage |
| B-perm | usage permuté à chaque pas | même distribution de poids, lien entrée↔usage rompu |

v2, A, A-jet et A-redir partagent la même suite de dérive (générateur indépendant de la soupe). Les comparaisons
sont donc appariées.

**Protocole :**
- 4 sources (S#0, S#1, T#0, T#1) × 8 graines (celles de `k8_survivor`, dont S#0 r5) × 6 bras = 192 runs ;
- 512 époques d'établissement, puis 8192 époques de dérive, soit 1024 pas ;
- N=1024 ;
- mort = plus aucun copieur vérifié dans la population entière (contrôle toutes les 64 époques) ;
- naissance = au moins 51 copieurs ;
- fertilité finale = émergence depuis le bruit sous la table finale figée (2048 époques, dominant non cristallin).

Calcul : 238 s sur 8 cœurs.

## Résultats (`run.txt`)

| bras | part vivante | morts/run | lignée morte en fin | stérilisation méd. (dé) | fertile en fin | dist. table | écritures |
|---|---|---|---|---|---|---|---|
| v2      | 0.150 | 1.66 | 31/32 | 1152 | 1/32 | 455 | 0 |
| A       | **0.079** | 1.38 | 31/32 | **256** | 0/32 | 463 | 981 |
| A-jet   | 0.163 | 1.69 | 31/32 | 960 | 3/32 | 455 | 0 |
| A-redir | 0.101 | 1.41 | 32/32 | 896 | 0/32 | 599 | 978 |
| B       | **0.476** | 3.16 | 27/32 | 6208 | **8/32** | 455 | 0 |
| B-perm  | 0.227 | 2.03 | 32/32 | 1664 | 1/32 | 457 | 0 |

Continuité stricte : 0/32 dans tous les bras.

Comparaisons appariées (permutation à signes) :

| comparaison | p |
|---|---|
| A < v2 | 0.0055 |
| A < A-jet | 0.0069 |
| A ≈ A-redir | 0.50 |
| B > v2 | <5e-5 |
| B > B-perm | 0.0001 |
| B-perm vs v2 | 0.07 |

Par source, B gagne sur S#0 (0.71 contre 0.13), S#1 (0.63 contre 0.16) et T#1 (0.49 contre 0.25), mais pas sur
T#0 (0.07 contre 0.06).

## Preuve d'émergence ? Non

On regarde les écritures acceptées pendant que la lignée vit, sur 32 runs. Une entrée est dite **utilisée** si le
dominant du moment l'exécute quand il se copie.

| | A | A-redir |
|---|---|---|
| écritures pendant la vie | 2028 | 2828 |
| sur entrée utilisée | 333 (attendu 45.6 au hasard uniforme, ×7.3) | 43 (attendu 48.8, ×0.88) |
| dont effectives (valeur changée) | 93 sur 832 (attendu 19.8, ×4.7) | 39 sur 2649 (×0.85) |
| cassent le dominant / le réparent | **30 / 8** | 23 / 0 |
| pas de dérive défaits | 263/32768 (0.8 %) | 1930 (5.9 %) |

L'enrichissement brut ×7.3 est surtout un **biais mécanique**. `WRITE_LAW` adresse loi[phase][b], et le registre b
contient le plus souvent des octets de la soupe, c'est-à-dire du dominant. Or les entrées « utilisées » sont
justement (phase, octet du dominant).

Pour le mesurer, on prend un nul **« composition »** : la cible est n'importe quelle (phase, octet présent dans le
dominant). Contre ce nul, il reste ×1.59 (333 contre 209.6, z≈8.5), et seulement ×1.35 pour les écritures
effectives (93 contre 69.1, z≈2.9). 72 % des écritures sur entrées utilisées réécrivent la même valeur : c'est une
copie, pas une réparation.

Et quand l'écriture change vraiment une loi utilisée, elle casse le dominant presque 4 fois plus souvent qu'elle ne
le répare. Il n'y a pas de gel de la physique : la distance finale à la table de départ est de 463 en A contre 455
en v2, et la soupe ne défait que 0.8 % des pas de dérive.

**Artefacts mesurés :**
- **auto-destruction :** 17 des 44 morts du bras A (39 %) surviennent dans les 128 époques qui suivent une écriture
  qui casse le dominant. Le chiffre est de 9/45 pour A-redir et de 0/54 pour A-jet ;
- **gel :** absent.

**Le meilleur « réparateur » :**
- **Cas :** T#1, graine 4, dérive 120. La loi tab[1][ff] passe de HALT à INC_A, et le dominant recopie.
- **Écrivain :** `0c0000413959064a8155155f1007c8e657e27f9aa3975bff0f0070e400feed00`. C'est un copieur vérifié, à
  Hamming 24 du dominant.
- **Ablation :** on reprend le même run à cette époque, avec 8 bifurcations de dérive.
  - écritures gardées : part vivante 0.138, 0/8 vivant en fin ;
  - écritures jetées : 0.386, 2/8 vivant en fin (p=0.11).
- **Conclusion :** cette réparation était un accident heureux, pas un comportement de gardien.

## Verdict

La vie ne protège pas ses lois de façon émergente dans ce monde. Lui donner la plume abrège la vie : un accès en
écriture aux lois est un nouveau canal de mutation, que la sélection au niveau du réplicateur ne sait pas
discipliner. En effet, une écriture de loi profite ou nuit à **toute** la soupe à la fois. Le gain n'est pas
approprié par l'écrivain, et la sélection ne voit rien.

La protection qui marche (B) est collective et passive : les lois qu'on exécute bougent moins. La preuve qu'elle
tient au lien avec l'usage est B-perm, qui ne fait guère mieux que v2. Mais B est posé dans la physique.

## Reproduire

    ./experiments/universe_search2 race --seed 2 --save /tmp/k8_fin2.bin     # finalistes v2 (~4 min)
    make experiments/law_keepers && ./experiments/law_keepers selftest
    ./experiments/law_keepers run --load /tmp/k8_fin2.bin --seeds 8 --edrift 8192 --fert 1   # 238 s, 8 cœurs -> run.txt

## Prochaine étape

Rendre la protection *appropriable*, puisque c'est l'appropriation qui manque. Il s'agit de remplacer la table
globale par des **lois locales** :
- chaque région (ou chaque génome) porte sa propre copie de la table ;
- cette copie est héritée avec le réplicateur et dérive aussi.

Une lignée qui garde ses lois en profiterait alors seule, et la sélection pourrait faire émerger des gardiens. On
testera A local contre A global et contre A-jet, avec les mêmes témoins. On testera aussi si B émerge tout seul
quand des lois « coûteuses à changer » sont héritables.
