# Saison sur les pompes

`--season-amp` ne modulait que la repousse de base. Sur un monde pavé,
la ferme et la technologie versent bien plus : une saison sur la
repousse ne crée pas de mort. L'amplitude 1, période 4 000, a déjà
éteint un fork de vie-monde au premier creux.

`--saison 1` applique le même facteur aux ajouts de la ferme et de la
technologie. Au creux, une fraction déterministe des cases ne reçoit
pas l'ajout (`(x, y, t)`). Au sommet, la pompe reste au plus égale à
elle-même : pas de surplus ajouté. Amplitude 0 : toutes les cases
passent, comme si le drapeau était éteint.

Parent : vie-forces-societes. Amplitude 0,35, période 24 000. Binaire :
`experiments/terrarium_saison`.
