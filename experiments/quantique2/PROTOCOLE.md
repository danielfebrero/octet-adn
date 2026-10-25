# Q2 — ensembles qui se recopient par l'observation

Préenregistré le 25 octobre 2026, avant la série. Suite de Q1 (`experiments/quantique`), dont les trois
mondes vivants, l'ELF et les sources ne sont pas touchés. Moteur séparé : `monde2.cpp`.

## Ce que Q1 a montré, et ce qui bloque

Q1 a un champ de phase unitaire, un noyau unique d'attraction–répulsion, des liens durables et une
projection de Born déclenchée par une boucle réciproque ensemble ↔ point. Ses propres résultats posent trois
limites : (1) le graphe des liens percole jusqu'à 64 octets, il n'y a donc pas d'individus ; (2) la valeur
projetée est seulement écrite dans le journal, elle n'a aucune conséquence ; (3) ni filiation ni hérédité.

## Les quatre changements de Q2 (chacun a un réglage qui redonne Q1)

1. **Bain thermique** (Langevin, frottement γ = 0,5, température T = 0,6) : les liens se font et se défont.
2. **Queue répulsive à longue portée** ajoutée au même noyau (`rep·exp(-r/ξ)/r`, coupée à 5, sans dépendance
   aux valeurs). C'est la forme SALR, qui donne des amas de taille finie au lieu d'une seule goutte. `rep = 0` = Q1.
3. **Le champ est relaxé par le même bain** : après l'étape unitaire de Q1, `psi ×= exp(-dt·cool·V)` puis
   renormalisation. Le champ tend vers l'état de Boltzmann du potentiel local au lieu d'osciller. `cool = 0` = Q1.
   Ce n'est plus une évolution unitaire : la norme est conservée par renormalisation, pas par construction.
4. **La projection a une conséquence (empreinte)** : la valeur tirée est écrite dans l'octet libre (hors de tout
   ensemble) le plus proche du point, à moins de 2,5. L'octet garde la marque de l'ensemble propriétaire.
   L'identité d'un ensemble persiste d'une étape à l'autre (recouvrement ≥ 2 octets), et la boucle d'attention
   s'attache à cette identité, non plus à l'ensemble exact des membres.

Naissance d'un ensemble : **fission** (sa majorité vient d'un ensemble dont l'identité est reprise par une autre
part), **empreinte** (au moins la moitié de ses octets portent la marque d'un même ensemble, depuis moins de
1 024 ticks), sinon **spontanée**. La génération d'un né = celle de son parent + 1.

Calibration (graines 1 à 4, déclarée, jamais réutilisée) : sans queue répulsive, tout condense en 1 ou 2 gouttes
à toute température essayée (0,05 à 1,5). Champ unitaire : la valeur projetée reste à 52–56 de la phase du
propriétaire (hasard 64) quel que soit le gain (1 à 256). Relaxation cool = 100 : 13. Répulsion 1,5 : ~16 ensembles
de ≤ 5 octets.

## Réglage figé

`--n 96 --box 12 --grid 6 --modes 64 --period 16 --temp 0.6 --gamma 0.5 --rep 1.5 --xi 2 --rcut 5 --spec 0.5
--cool 100 --gain 1 --bind-r 1.4 --window 1024`, 16 384 ticks, graines 1001 à 1020.

Bras (même graine = mêmes octets de départ) :

| bras | empreinte | champ |
|---|---|---|
| E | valeur projetée (`--imprint 1`) | relaxé (cool 100) |
| H | valeur tirée au hasard, indépendante du champ (`--imprint 2`) | relaxé |
| S | aucune écriture (`--imprint 0`) | relaxé |
| U | valeur projetée | unitaire comme Q1 (`--cool 0`) |

## Mesures et critères

Par monde, depuis `naissances.tsv` (chaque né : distance circulaire de sa phase à celle de son parent,
`d_parent`, et à celle d'un autre ensemble vivant tiré au hasard, `d_hasard` ; distances 0–128) :

- **H_e** = 1 − Σ d_parent / Σ d_hasard sur les naissances par empreinte (lignes avec les deux distances).
- **L** = profondeur moyenne, sur toutes les naissances, de la chaîne de générations successives où chaque né est
  plus proche de son parent que de l'ensemble tiré au hasard (`d_parent < d_hasard`). Sous l'hypothèse nulle, chaque
  maillon a une chance sur deux (profondeur moyenne ≈ 1). Changé avant la série : la plus longue chaîne (un maximum)
  croissait avec le nombre de naissances et valait 12 dans le témoin H au test de chaîne (graines de calibration 1–2).

Critères principaux, contrastes appariés par graine, intervalle t à 97,5 % (Bonferroni sur 2), 19 ddl :

- **C1** : H_e(E) − H_e(H) > 0.
- **C2** : L(E) − L(H) > 0.

Les deux tenus → « les ensembles Q2 sont des unités d'évolution minimales : ils se multiplient, transmettent une
valeur héritable et varient par le bruit de copie ». Un seul → partiel, dit tel quel. Aucun → non.

Secondaires, sans décision : ordre de phase final (moyenne des 8 derniers relevés), nombre et taille des ensembles,
H des fissions, bras S et U, mondes éteints.

## Ce que le résultat ne dira pas

Pas de sélection démontrée : aucune phase n'a d'avantage écrit dans la règle. Une invasion par une phase est une
dérive ou une course de vitesse, pas une adaptation. Pas de métabolisme au-delà du bain, pas de conscience. Le mot
« quantique » nomme toujours un modèle hybride, de champ moyen, désormais ouvert sur un bain.
