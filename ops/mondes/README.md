# Reprise des mondes sans interruption globale

Le superviseur déployé est `/workspace/ops/octet-mondes/reprise.sh` (chemin hôte :
`/data/eternalgrok-grok-workspace/ops/octet-mondes/reprise.sh`). Son inventaire
`mondes.tsv`, à côté du script, est relu à chaque passage. Il contient 18 mondes.

Pour inscrire un nouveau monde déjà lancé, ajouter sa paire ELF/répertoire à
l’inventaire et remplacer le fichier atomiquement, puis maintenir la copie de ce
répertoire dans Git. Aucun restart de service ni de conteneur n’est nécessaire.
Le contrôle prend au moins 20 secondes plus le temps de parcours de `/proc`.
`reprise.sh --check` valide noms uniques, ELF exécutables et checkpoints présents.
Un monde absent est repris par le même ELF avec `run --dir DIR --ticks 0`, sans
choc ; aucun monde neuf n’est créé si son checkpoint manque.

Le script prend un verrou exclusif ; les mondes enfants ne gardent pas ce verrou.
Ne pas redémarrer le service pour changer la liste : son unité existante emploie
`KillMode=control-group` et pourrait alors arrêter les mondes qu’il aurait repris.
Le conteneur historique conserve sa commande de lancement de 14 mondes ; les
quatre suivants sont couverts par le service hôte indépendant.

## Intégration du 25 octobre 2026

Fusion de `codex/trois-approches` dans main ; les sept autres commits Codex et
les profils capture/parenté/sénescence étaient déjà sur main. Les conflits de
rendez-vous conservent l’API et la compatibilité checkpoint de main. Les trois
éprouvettes supplémentaires restent isolées ; leurs résultats antérieurs sont
archivés, leurs chiffres ne constituent pas une nouvelle mesure du merge.
Le banc corrections est adapté à l’API comportementale à trois arguments.

Validation : compilation isolée ; selftests perception, sélection, symbiose et
corrections ; 10/10 hashes historiques ; selftests rencontres avec reprise
identique pour les nouveaux profils ; garde sur fixtures, 14 cas sans échec.
La garde réelle refuse les mondes non orphelins (PPID différent de 1) et un
/proc partiellement illisible : elle n’est pas une preuve de l’hôte ici.
Contrôle indépendant sur /proc : les 18 PID, starttime, chemins ELF et inodes sont
inchangés après remplacement du seul superviseur. Son cgroup ne contenait que
le shell de surveillance et son sleep, contrôlés avant activation.
