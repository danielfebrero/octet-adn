# Phase écologique prospective, protocole E1

Enregistré après la première campagne du banc, avant tout lancement écologique.
Le génome general a échoué à exploiter l'indice dans ce banc ; cela ne préjuge
pas d'une population évoluant par survie/reproduction. Aucune cible chiffrée des
campagnes M1/M2 ne change.

**Correction de portée :** la porte « candidat doit passer avant toute écologie »
ajoutée par l'implémentation v1 était plus restrictive que le plan validé. Le
plan exigeait de valider le *dispositif*, puis de tester la sélection dans un
nouveau monde. La porte v1 reste rapportée telle quelle dans ses résultats ;
elle ne bloque pas cette phase distincte, après validation des dispositifs M1/M2.

## Milieu et fondateurs, fixés avant démarrage

E1 utilise M1 : indice fiable 75 %, coût 1, délai 4, bonne route +20,
métabolisme 10 par épisode. Une seule route irrévocable, aucune récompense
secondaire, aucun voisin. H est retiré indépendamment pour chaque individu et
épisode. Ce milieu teste l'usage individuel d'information, pas une société.

64 fondateurs : 16 copies de chacune des seeds préexistantes general, echo,
neural, forager. Aucun T8, oracle ou programme de composition introduit. Énergie
initiale 160. Pas de nourriture gratuite ni de réinjection de population.
Plafond 160, sans élimination à l'atteinte du plafond. Un survivant d'énergie
≥120 peut transférer 60 à un enfant après son épisode. Chaque octet de l'ADN
germinal est remplacé par un octet uniforme avec probabilité 1 %. Longueur
constante ; pas de croisement ni édition dirigée. Le soma n'est pas hérité.
Mort lorsque énergie ≤0. Aucun score de sélection externe : seuls le paiement,
le métabolisme et la consultation changent l'énergie.

Graine évolutive 20261025. RNG du milieu adressé par individu/épisode ; RNG des
mutations distinct. Épisodes complets par individu ; un enfant commence au
cycle suivant. Sauvegarde après chaque cycle. Pause d'une seconde entre cycles,
sans effet sur le temps simulé. Instance Docker distincte, CPU limité et code
monté en lecture seule ; aucun des 18 vivants n'est redémarré.

## Mesures, preuves, limites

`series.tsv` : ticks (10 par épisode), population, génotypes germinaux,
naissances/morts cumulées, succès/consultations/omissions du cycle, énergie
moyenne. `events.tsv` : enfant, parent, hash du génome, décès. `checkpoint.bin` :
état complet de chaque VM, soma/germen, énergie, âge, RNG évolutif, identifiants,
compteurs et offsets des journaux. Les offsets permettent de retirer uniquement
les écritures postérieures au dernier checkpoint après un crash. Checkpoint
temporaire synchronisé puis renommé atomiquement. Reprise avec le même ELF.

Un processus éteint ne signifie pas automatiquement extinction : lire le
checkpoint et le dernier relevé. Une population nulle est une extinction,
jamais un prétexte pour recréer des fondateurs. Ne pas injecter de mutants
dirigés après échec. Le programme peut s'arrêter normalement après extinction.

La croissance, une mutation, une lignée durable ou un meilleur score ne sont
pas une preuve de mémoire utile. Après 256 épisodes (ou extinction), extraire
les génomes germinaux pour une évaluation gelée M1, mêmes témoins, 32 nouvelles
graines appariées, critères inchangés. Ne pas réutiliser les graines qui ont
sélectionné ces lignées. Un regard antérieur rapporte seulement démographie et
faits observés, sans revendiquer émergence ou raisonnement.
