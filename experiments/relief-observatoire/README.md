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
- Laboratoire du hasard : xorshift64*, état interne hexadécimal, saut exact
  logarithmique par exponentiation de la transformation linéaire sur GF(2),
  puis cinq sorties. Aucun RNG ni checkpoint de production n'est modifié.

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
JS analysé et exécuté dans Node22 : 900 comparaisons saut/pas direct sur trois
états et composition de grands sauts. Rendu Chromium isolé 412×1800 vérifié
sur un instantané réel tick131400 ; carte, courbes et microscope rendus.
HTTPS page200 ; API publique sans signature403.

Moteur vivant conservé : SHA256
`c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`,
conteneur octet-relief démarré à19:56:08Z, toujours en cours après déploiement.
Le rejeu de l'écologie est déjà vérifié par les tests du moteur Relief ; cette
mini-app n'initialise aucune écologie et ne crée aucun laboratoire vivant.
