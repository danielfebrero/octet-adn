# Causales

Une caractéristique tenue ne faisait que se mesurer. `--causales 1`
donne un effet au signal du tick, sans tirage de plus.

Parler aux siens déplace une nourriture vers le premier voisin de la
même espèce qui a de la place (la somme ne change pas). Parler aux
autres pose une marque (quartet bas de l'espèce, 1 si ce quartet est
nul) sur la case. Au tick suivant, le quartet haut du stimulus devient
cette marque : elle est lisible. Le prédateur, s'il y en a un, reste
prioritaire.

`--causales 0` ne touche ni la nourriture ni le stimulus. Parent :
vie-traits. Binaire : `experiments/terrarium_causales`.
