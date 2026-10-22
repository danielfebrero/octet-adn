# Lois locales héritées (`experiments/local_laws.c`)

Question (Dani) : la vie protège-t-elle ses lois, de façon **émergente** ? Suite de `law-keepers/` : la table
des lois n'est plus globale. Chaque individu porte **sa propre table**, et une copie la transmet.

## Dispositif
- **Table propre complète** : kper×256 octets par individu (pas de sous-ensemble). La paire (pa, pb) s'exécute avec
  les lois de pa.
- **Héritage** : quand pa réécrit au moins L/2 octets de pb, pb reçoit une copie de la table de pa. C'est une règle
  mécanique de la physique : aucun oracle de fidélité, aucune récompense.
- **Dérive par individu** : chaque individu reçoit un coup de dérive sur une entrée avec une probabilité de 0.9/K par
  époque (K=8). Chaque lignée subit donc exactement la dérive de v2, mais les coups sont indépendants d'un individu
  à l'autre, ce qui permet aux lignées de diverger. Avec une dérive « par copie », la dérive serait liée au taux de
  reproduction et une lignée stérile serait immunisée, ce qui biaiserait le test. Les paramètres globaux
  (rcost/wcost/steps/mut) dérivent comme en v2.
- **Jeton d'écriture** : chaque coup de dérive donne à l'individu un jeton d'écriture qui ne s'accumule pas
  (« inertie des lois », comme dans law-keepers). Un enfant hérite de la table, sans jeton.
- `READ_LAW`/`WRITE_LAW` (1 octet) agissent sur la table propre de l'exécutant.
- **Bras** :
  - A-local : écritures actives ;
  - A-local-jet : écritures jetées ;
  - A-local-redir : écritures redirigées à une position aléatoire de la table propre ;
  - local-sans-W : dérive et héritage seulement ;
  - références v2 et A global, sur les mêmes graines.
- **Protocole** : 4 sources × 8 graines (dont S#0 r5), N=1024, établissement 512 puis dérive 8192 époques, soit
  1024 pas de dérive par lignée (8× le protocole de base, dans le budget).
  Les bras locaux partagent exactement la même suite de dérive, ce qui permet de les apparier.
- **Anti-triche (selftest)** :
  - 1800 tables locales aléatoires, dont 600 à une seule loi, 653 181 pas : 0 écriture de plus d'1 octet ;
  - aucune table à une seule loi ne produit de copieur ;
  - le copieur fait main transmet sa table, un génome inerte ne la transmet pas ;
  - `WRITE_LAW` ne touche que la table propre.
