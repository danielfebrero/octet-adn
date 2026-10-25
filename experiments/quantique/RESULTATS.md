# Q1 — premiers résultats, 25 octobre 2026

Le moteur indépendant fonctionne : 3 dimensions spatiales, ticks, octets sans
VM, un seul potentiel d'attraction–répulsion et un champ de possibilités
complexes. Une fixation exige un retour causal mesurable du point vers au
moins deux membres d'un assemblage durable.

Ce monde est une construction hybride de champ moyen, inspirée du quantique.
L'infini est approché par un maillage et une résolution de phase finis. Les
octets ne raisonnent pas ; aucune vie ni conscience n'est démontrée.

## Les 20 essais prévus, tous conservés

4 096 ticks par essai, 64 octets, mêmes conditions initiales par graine.
P désigne des ticks consécutifs de boucle du même assemblage ; 0 désactive
les projections. Le graphe des assemblages est mesuré, il n'attache rien.

| Résolution de phase | P | Graine 101 | 202 | 303 | 404 |
|---|---:|---:|---:|---:|---:|
| 256 | 0 | 0 | 0 | 0 | 0 |
| 256 | 4 | 26679 | 26634 | 27170 | 29130 |
| 256 | 16 | 1578 | 2386 | 2131 | 1524 |
| 256 | 64 | 31 | 38 | 0 | 35 |
| 128 | 16 | 2200 | 1496 | 2025 | 1740 |

Les nombres sont les fixations de points, pas des organismes ni des actes
conscients. Le témoin reste à zéro. À P=64, la graine 303 ne fixe aucun point :
sa boucle maximale dure 49 ticks. Cet échec n'est pas éliminé.

Des liens attractifs de huit ticks existent dans tous les bras, y compris
sans observation. Tous atteignent temporairement une composante de 64 octets ;
cela indique aussi que notre définition forme facilement un réseau étendu.
Elle ne suffit pas à distinguer des organismes autonomes ou des sociétés.
À la fin, les composantes les plus grandes comptent de 53 à 64 octets.

La résolution 128 change les nombres de fixations et les trajectoires.
La convergence vers un espace idéal infini n'est donc pas établie. Aucun
avantage biologique, apprentissage ou complexité croissante n'est revendiqué.

## Vérifications

Compilation C++17 stricte sans avertissement. Contrôles passés : dérivée du
potentiel, signes attractif/répulsif, coupure continue, réciprocité des paires,
normalisation unitaire, support et fréquences de la projection de Born,
aucune projection dans le vide ou pour un octet seul, formation préalable
du lien et témoin sans projection. Déterminisme bit à bit sur la même
plateforme ; reprise exacte après 113 + 143 ticks comparée à 256 ticks ;
journaux simulant un crash purgés au checkpoint ; nouvelle configuration
refusée lors d'une reprise.

L'erreur maximale de norme sur ces 20 essais est inférieure à 2,5×10⁻¹¹.
Les projections ne conservent pas nécessairement l'énergie et la grille
constitue un substrat fixe. Les seuils numériques restent des choix de modèle.

## Reproduction et provenance

Sources : `monde.cpp`, protocole des essais et moteur au commit `c2b24a7`.
La version exacte du protocole utilisé est copiée sous
`resultats-20261025/PROTOCOLE-ESSAIS.md` et son SHA-256 concorde avec le manifeste.
La modification ultérieure du protocole remplace seulement le conteneur
commun prévu par trois conteneurs indépendants à 0,15 CPU chacun.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static monde.cpp -o monde
python3 essais.py /chemin/monde --output /chemin/resultats-nouveaux
```

`resultats-20261025/resultats.tsv` contient les 20 lignes complètes. Les
40 journaux complets des événements et mesures, les empreintes des
checkpoints et les empreintes source/ELF/protocole sont conservés avec eux.
Les checkpoints complets restent sous `experiments/quantique-runs/`.

`lancer.sh` crée exclusivement de nouveaux conteneurs et refuse de remplacer
un nom ou répertoire existant. Aucun ancien processus ou binaire n'a été
arrêté, remplacé ou relinké.
