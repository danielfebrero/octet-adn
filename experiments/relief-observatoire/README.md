# Relief · Observatoire et microscope

Mini-app persistante publiée le 25 octobre 2026 : bouton **Relief** dans Mini-app,
URL `https://mcp.cdxxotus.com/relief/`, artifact
`c4115486-4cb7-497c-aad1-7c07a7ddc72c`.

## Instruments

- Carte 128×64, chaque pixel agrégeant 8×8 cases du monde : population,
  nourriture, altitude, constructions, roches. Loupe locale 17×17 cases.
- Séries depuis le début, sélection de métrique, renouvellement entre les
  deux derniers points affichés (fenêtre indiquée, pouvant être décimée).
- Organisme sélectionné par case ou identifiant : parent, lignée, état
  corporel, génome, mémoire, perceptions, trace du décodage de huit instructions
  au plus et comparaison aux perceptions/mémoire neutralisées.
- Suivi entre sauvegardes : positions, âge, énergie. Un absent est signalé,
  jamais remplacé silencieusement par un autre organisme.
- **Monde 3D** (26 octobre) : terrain aux altitudes réelles agrégées 8×8,
  tous les organismes du checkpoint avec identités exactes, couleurs par lignée
  ou énergie, calques nourriture/constructions, structures et roches.
  Corps stylisés dans la zone locale (49×49), panorama, immersion, orbite
  automatique, navigation tactile, sélection, suivi et accès au microscope.
- Anneaux de lecture des événements réels, datés : naissances, décès,
  constructions, dépôts, attaques, dons et parasitisme selon les traces présentes.
  La sélection est limitée à 80 événements côté API, avec au plus quatre traces
  de chaque type dans la liste visible. Les anneaux **rejouent en boucle** ces
  traces ; ils ne représentent pas des actions survenant à l'instant du rendu.
- La section **Hasard est supprimée**, ainsi que son code et ses contrôles.

Les déplacements entre deux sauvegardes d'un même identifiant sont interpolés
visuellement sur huit secondes (écarts de moins de 40 cases seulement ; les
plus grands sont affichés sans interpolation). Le chemin réel entre relevés
reste inconnu. Les halos sont décoratifs, les corps sont stylisés et la hauteur
du terrain est réglable : aucune morphologie ou action n'est déduite de ces
choix de rendu. Un organisme absent n'est jamais remplacé dans le suivi.
Les options « Animations » et le réglage système de réduction du mouvement
coupent halos animés, transitions et anneaux. Sans WebGL, carte et microscope
restent accessibles. Les modules Three.js 0.180.0 et OrbitControls sont servis
localement avec leur licence MIT ; aucun CDN n'est requis à l'ouverture.

L'observation porte sur les checkpoints atomiques, généralement toutes les
600 ticks (~60 s). La page les consulte toutes les 15 s lorsqu'elle est visible.
Elle affiche le tick, la date et l'âge de la sauvegarde. Ce n'est pas une trace
exhaustive des instructions effectivement exécutées : la prédiction locale
précède l'entretien et l'évolution du milieu du prochain tick. Les conséquences
entre sauvegardes ne sont pas attribuées causalement. Pas de mesure de générations
généalogiques dans cette version, seulement âge et lignée fondatrice.

## Isolation et exploitation

`read.c` inclut le code du moteur et réutilise `load`, qui vérifie ABI,
checksum, occupation et conservation. Son main est distinct : il n'appelle
ni init, ni run, ni save, ni action ; choose ne touche qu'une copie d'Agent.
Le monde est monté **lecture seule** dans le conteneur du serveur. Aucune
route de commande n'existe. Le moteur ELF005 et son conteneur n'ont pas été
modifiés ni redémarrés.

Le serveur Python standard utilise un lecteur C statique séparé ; Python ne
participe pas à la physique. Accès aux données par signature Telegram HMAC,
âge max 24 h, liste de comptes autorisés. Le token provient de l'environnement,
jamais du JS ni d'un fichier. La page HTML publique ne contient aucune donnée.
Préférences et sélection conservées dans localStorage du WebView.

```sh
cd /workspace/produits/octet-adn/experiments/relief-observatoire
cc -O2 -std=c11 -Wall -Wextra -Werror -static -o reader read.c -lm
sh run.sh # crée relief-observatoire, échoue s'il existe déjà
docker exec relief-observatoire python3 /app/check.py
```

`deploy_route.py` ajoute uniquement `/relief/*` sur `mcp.cdxxotus.com` aux
routes Caddy actives, préserve tous les autres handlers et ajoute la même route
au Caddyfile de redémarrage. Exécution en réseau container:Caddy avec dossier
deploy monté. Sauvegardes `relief-caddy-before.json`, `Caddyfile.before-relief`.
Ne pas recharger un ancien Caddyfile pour déployer cette app.

## Vérification de livraison

Compilation C stricte réussie. Authentification : sans signature, mauvais compte
et session expirée refusés. Lecture réelle tick130800 : matière76840404,
8192 pixels, séries, génome64, mémoire8, perceptions8, sélection par identifiant.
Le laboratoire du hasard avait été vérifié le 25 octobre ; il est désormais retiré. Rendu Chromium isolé 412×1800 vérifié
sur un instantané réel tick131400 ; carte, courbes et microscope rendus.
HTTPS page200 ; API publique sans signature403.

Moteur vivant conservé : SHA256
`c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`,
conteneur octet-relief démarré à19:56:08Z, toujours en cours après déploiement.
Le rejeu de l'écologie est déjà vérifié par les tests du moteur Relief ; cette
mini-app n'initialise aucune écologie et ne crée aucun laboratoire vivant.


## Validation 3D du 26 octobre 2026

- Compilation stricte du lecteur et intégration dans le seul observatoire.
- Tick 531000 : 65536 identités uniques, coordonnées dans le monde,
  correspondance avec les occupants de la loupe ; aucune trace postérieure au
  checkpoint exposée. Le serveur refuse une lecture si le checkpoint est
  remplacé pendant l'extraction. Cache borné à une population complète.
- Tests d'accès Telegram, population 3D, modules et réponse gzip :
  `docker exec relief-observatoire python3 /app/check.py`.
- Syntaxe Node22 : `node check-js.cjs`.
- Chromium isolé, données réelles interceptées pour le rendu : mobile 412×915
  et bureau 1440×1000. Panorama, immersion, calques, hauteur, agrandissement,
  animations, carte et microscope vérifiés sans erreur JS/WebGL ; largeur
  mobile de 412 pixels, sans débordement. Aucun secret dans les fixtures.
- HTTPS `/relief/` : 200 ; `/relief/data` sans signature Telegram : 403.
- Aucun redémarrage de `octet-relief`, ELF005 inchangé. Seul le serveur
  d'observation a redémarré pour charger sa nouvelle API.

Le rendu a été testé dans Chromium avec accélération logicielle ; la cadence
sur un téléphone physique n'a pas été mesurée. Le plafond visé est 30 rendus/s,
avec résolution limitée à 1,5× et arrêt du rendu quand la page est masquée.
