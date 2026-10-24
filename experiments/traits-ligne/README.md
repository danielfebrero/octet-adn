# Caractéristiques

Une niche n'est plus une case d'une grille décidée à l'avance.

La grille fixe compte cinq gestes — bouger, manger, tourner, se diviser,
émettre — chacun coupé en trois (rare, un peu, beaucoup). Diviser et
émettre étaient souvent à zéro, donc les niches qui tenaient se lisaient
comme « manger, tourner, bouger ». Attaquer, céder, cultiver, hésiter,
parler aux siens ou aux autres n'entraient pas dans l'identifiant.

Avec `--traits 1`, un fait devient caractéristique seulement s'il sépare
la population. Chaque tick note les opcodes vraiment exécutés, plus six
relations : hésite, cultive, entre siens, entre autres, énergie basse,
case nue. Au relevé, on garde un fait dont la fréquence est entre 5 % et
95 %, et qui n'est pas la copie d'un fait déjà retenu (corrélation au-delà
de 0,80). Il faut dix relevés d'affilée. Au plus huit caractéristiques
tiennent en même temps. La niche est le masque de celles que la créature
porte. Elles peuvent naître et se perdre (`trait_ne`, `trait_perdu`).

`--traits 0` ne pose pas ce relevé et ne tire aucun nombre de plus. Le
checkpoint sans traits reste `OADNTER7`. Avec traits, l'écriture est
`OADNTER8` : le corps est celui d'OADNTER7, suivi de l'état de la
découverte. Traits n'est pas dans Params. Le binaire est
`experiments/terrarium_traits` : les mondes déjà lancés gardent le leur.
