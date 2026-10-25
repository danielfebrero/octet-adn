# Revue indépendante Relief

Périmètre : nouveau moteur uniquement. Aucun ancien monde touché.

## Critères avant lancement durable

- Toute transformation de matière doit déplacer des unités entières entre stocks explicitement comptés. Naissance, mort, coût et attaque ne doivent ni créer ni perdre de matière ; l'apport externe doit être comptabilisé.
- Les dépenses ont un effet biologique : perception, mémoire et action consomment des ressources ; un refus pour manque de ressources ne doit pas réaliser l'action gratuitement.
- Reprise déterministe : un run de N ticks et deux runs N/2 + reprise doivent produire les mêmes états, compteurs et RNG.
- Un checkpoint incomplet, corrompu ou incompatible est rejeté sans semer de fondateurs. Aucun mode de reprise ne doit provoquer de réinitialisation implicite.
- L'extinction reste extinction. Aucun fondateur n'est réintroduit lors de la reprise.
- SIGTERM sauvegarde un checkpoint atomique ; un fichier temporaire écrit partiellement ne remplace pas le dernier état cohérent.
- Population, dimensions, indices et tailles de checkpoint bornés avant les allocations et accès.
- Le phénotype dépend du génome, perception et mémoire ; les interactions doivent avoir des causes matérielles, sans compteur de société récompensé.

## État initial

Répertoire du moteur vide au premier contrôle ; revue d'implémentation et tests en attente.

## Revue achevée : build-003

Verdict : aucun défaut bloquant restant trouvé après corrections, pour un lancement borné et instrumenté. Cela n'établit ni émergence de sociétés ni viabilité indéfinie.

### Défauts corrigés pendant la revue

1. Prédation près du plafond de stock : la victime perdait jusqu'à 4 unités avant que le gain du prédateur soit plafonné. L'espace disponible est désormais appliqué avant le retrait. Le test initial utilisait 0xf9 et visait une autre direction : corrigé en 0xc9, qui rencontre effectivement la victime.
2. Diffusion : la parité couplée à l'axe excluait définitivement certaines arêtes. Le code courant sélectionne l'axe puis la parité de la coordonnée avec une phase distincte ; toutes les arêtes éligibles sont couvertes sur quatre ticks.
3. Les instructions supplémentaires du décodage et le nombre d'octets mémoire actifs participent désormais au coût énergétique.

### Vérifications indépendantes

- Compilation de la source sous ASan et UBSan puis exécution de tests d'intégration : aucun diagnostic.
- Test indépendant, aussi exécuté directement avec l'ELF immuable build-003 : checkpoint strictement identique après 1000 ticks continus et 419 + reprise + 581 ticks.
- Checkpoint tronqué d'un octet, bit retourné dans le payload, octet surnuméraire : tous rejetés ; fichier corrompu jamais remplacé ni réinitialisé.
- Zéro fondateur : run ne crée aucun agent et conserve tick=0.
- SIGTERM pendant une exécution : sortie code 0, checkpoint relisible et validé ensuite.
- Cas ciblé prédateur stock=4095, cible adjacente, transfert potentiel=4 : conservation exacte après correction.
- Lecture des opérations : construction/déconstruction/cache/gift/naissance/mort ont des transferts explicites de matière ; parasitisme et don déplacent effectivement énergie/stock. Le modèle ne récompense aucun compteur social.
- Dimensionnement et tailles contrôlés avant allocation du chargement ; checksum et taille exacte contrôlés ; occupation reconstruite avec validation des indices et chevauchements.

Artefacts de vérification : /tmp/relief-review-external.py ; /tmp/relief-review-build003.py ; /tmp/relief-review-harness.c. Données du dernier test : /tmp/relief-review-kyvistiz.

### Limites explicites

- Pas de preuve d'émergence ni de stabilité à long terme : le contrôle 32x32 à 1000 ticks montre 256 vivants, 193 naissances, 17 morts, conservation exacte, mais ce n'est qu'un petit essai.
- Le coût mémoire active_memory/4 est quantifié par tranches de quatre octets actifs ; 1 à 3 octets n'ajoutent pas d'entretien. Le décodage supplémentaire coûte tout de même de l'énergie.
- Le champ de pluie courant change très lentement : le terme temporel augmente de 31 chaque 64 ticks avant décalage de 24 bits. L'humidité représente donc surtout une géographie climatique persistante, avec saison solaire ; ne pas la présenter comme une météo mobile rapide.
- Cette revue ne certifie pas « 10 fois le CPU » : il faut distinguer taille/détail du monde et charge mesurée sur le VPS, avec quota explicite.
- Les logs conservent des lignes de série supplémentaires à l'arrêt borné hors cadence ; l'état final et les événements se rejouent exactement, mais les séries peuvent inclure des observations intermédiaires supplémentaires.

## Revue différentielle finale : build-005

Verdict mis à jour : build-005 validé pour lancement borné et instrumenté ; aucun défaut bloquant trouvé dans les modifications finales.

- La pluie est désormais mélangée avec un terme temporel multiplicatif de 2246822519, variant tous les 64 ticks. La limite de pluie quasi statique signalée pour build-003 est levée.
- Le décodeur compte les instructions effectivement exécutées. Les octets sautés par branche ne sont plus facturés comme instructions.
- Échantillonnage des interactions par hash pur tick/id/type, 1/256, sans appel RNG. Naissances, fondateurs et morts restent complets ; les compteurs des interactions demeurent exhaustifs.
- Pacing --tick-ms : horloge monotone et nanosleep restent hors de World, sans modification du RNG.
- Test indépendant sur ELF build-005 : 80 ticks rapides et 80 ticks à --tick-ms 2 donnent state.ck, events.tsv et series.tsv identiques octet à octet. Artefacts : /tmp/relief-pace-review-p0aga233.
- Toute la suite externe est repassée directement sur ELF build-005 : reprise1000 vs419+581 exacte, rejet des trois corruptions, extinction sans fondateurs, SIGTERM et validation réussis. Artefacts : /tmp/relief-review-iyh6gvjk ; script /tmp/relief-review-build005.py.
- Le test de couverture des quatre matchings de diffusion est désormais présent dans selftest.

Aucun ancien monde touché et aucun commit effectué par le reviewer.
