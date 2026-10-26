# Relief : comparaison sur copies, 26 octobre 2026

Étude du déblocage local des caches et de leur concentration, sans modification du vivant. Le rapport français est `RAPPORT.md`. `PROTOCOLE.md` a été commité/poussé avant les simulations comparatives ; son hash est dans `preregistration.json` et le manifeste des runs.

## Outils

- `state_tool.c` réutilise exclusivement le chargeur/validateur et l'ABI d'origine ; il transforme des copies et exporte état, génomes et lignées. Aucun tick ni fondateur créé par cet outil.
- `check.py` vérifie indépendamment l'ABI avec ctypes, les différences binaires permises, les deux règles, déterminisme, checksum et refus d'écrire dans le vivant, y compris par symlink.
- `run.py --execute` est le pilote de l'étude initiale (utilise la première capture déjà présente et capture une deuxième fenêtre naturelle) ; chaque simulation est le binaire ELF005 original sous Docker limité à0,75CPU/256Mio, sans réseau, sans montage du vivant.
- `analyze.py` reconstruit les cohortes prospectives par tous les liens parent/enfant. `--selftest` teste décès avec descendance, générations, séparation des ancêtres déjà vivants au départ, extinction absorbante et recensement de fin de tick.
- `summarize.py` recalcule les contrastes et recopies agrégées de `results/`.
- `reproduce.py` rejoue seulement les deux captures figées, sans lecture du vivant. Une reproduction déterministe n'est pas une réplication indépendante.

## Reproduire

Depuis ce dossier, sur le même VPS avec le mapping Docker hôte indiqué dans run.py :

```sh
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Werror state_tool.c -o build/state_tool
python3 analyze.py --selftest
python3 reproduce.py --inputs data --output data/reproduction-001
```

Le nouvel output doit être inexistant ; le script refuse de remplacer une expérience. Il utilise les deux `data/windowN/base/state.ck` conservés localement et vérifie l'empreinte complète de l'ELF005 avant tout run. Aucun `init` n'est invoqué. Les ressources et délais sont bornés comme dans le protocole. Une reprise manuelle plus courte est possible depuis l'un des checkpoints archivés ; les commandes exactes de chaque segment sont dans `data/run-manifest.json` (montage `/trial` à remplacer par le nouveau répertoire de copie).

Pour refaire les analyses des résultats conservés sans simuler :

```sh
python3 analyze.py data/window1
python3 analyze.py data/window2
python3 summarize.py
```

Pour répéter les tests de transformation, donner un nom de bilan neuf :

```sh
python3 check.py --source data/window1/base --result data/tests/check-repeat.json
```

## Artefacts

`data/` et `build/` sont ignorés par git. `data/windowN/base/` contient la capture initiale ; les bras control/unlock/redistribute contiennent le checkpoint initial, les sorties aux600/4096/8192ticks, journaux exhaustifs naissances/décès (interactions échantillonnées), exports et audits de chaque cellule modifiée. `cohort-results.tsv` contient les65536racines initiales de chaque bras au dernier horizon. Les fichiers et empreintes sont inventoriés dans `results/evidence-manifest.json`.

Les résultats agrégés, captures/provenance et rapport sont commis dans git. Les gros états restent locaux, bornés, sans aucun nouveau conteneur permanent. Ne pas supprimer ou modifier les sources du vivant pour reproduire.
