# Symbiose par paquets : l'unité transmise peut être une paire

## Pré-enregistrement — avant toute mesure

**Phrase.** Transmettre ensemble deux catalyseurs complémentaires peut faire
évoluer un métabolisme conditionnel que le brassage libre des partenaires défait.

**Prédiction.** Depuis des octets uniformes, une population de microcompartiments
qui transmet les deux génomes ensemble atteindra un rendement conditionnel élevé,
avec un avantage causal sur la transmission des mêmes membres brassés.

C'est un nouveau modèle de sélection collective, distinct de la détection d'un
motif, de la copie en soupe et du rendez-vous spatial. La nécessité de deux
catalyseurs est une loi posée ici ; leur dépendance n'est donc pas présentée comme
une origine spontanée de la symbiose. La reproduction est un rééchantillonnage
explicite du modèle, pas une copie écrite par les programmes ni une preuve de vie.

## Dispositif figé

- 32 graines, numérotées 0..31 ; 64 compartiments de deux génomes de 32 octets
  uniformes 0..255 ; aucun fondateur conçu ou issu des seeds n'est planté.
- Chaque impulsion présente un substrat `s`. Le moteur existant exécute **un tick
  VM réel** de chaque génome dans un bac à sable frais : A=B=ip=0, énergie 128,
  budget historique de 16 instructions, stimulus=s. La première émission est la
  réaction, lue modulo 4. Pas d'émission = pas de catalyse. STORE ne modifie que le
  soma local ; l'ADN de naissance est transmis.
- Une unité de produit requiert simultanément les deux émissions différentes
  `s mod 4` et `(s+1) mod 4`, dans n'importe quel ordre. Un membre seul ne fabrique
  jamais ce produit. La VM et l'ISA ne changent pas ; ce sont les réacteurs qui
  lisent EMIT. Les actions spatiales restent les actions réelles dans le bac
  isolé, sans nourriture ni partenaire ni division possible.
- 512 générations fixées. À chaque génération, les 16 substrats 0..15 donnent un
  nombre de produits dans 0..16. Chaque compartiment parent est rééchantillonné
  avec un poids `1 + produits` (survie de fond identique pour tous). À la naissance,
  chaque génome reçoit avec probabilité 1/2 une substitution uniforme d'un octet.
  Aucun autre opérateur, aucun curriculum, aucune prolongation après résultat.
- **Paquet** : deux membres d'un même parent sont transmis ensemble, avec leur
  ordre tiré au hasard. **Brassage** : même sélection et même mutation, puis tous
  les 128 membres sont permutés uniformément et réappariés à chaque génération.
  Les deux bras commencent avec exactement les mêmes génomes par graine.
- **Bruit** : mêmes générations et mutations, parents tirés uniformément, aucune
  sélection par les réactions. **Cristal** : les 128 génomes sont `14` répété
  (EMIT constant), sans mutation, sans sélection. Aucun n'a un second catalyseur.
- Évaluation finale : 128 substrats frais tirés uniformément dans 16..255, communs
  aux quatre bras d'une graine, jamais employés pour la sélection. Rendement =
  produits / (64 × 128). On mesure aussi les mêmes paires avec second membre
  remplacé par YIELD (ablation), puis avec le premier seul. Les tests causaux
  sont volontairement simples : le modèle exige deux catalyseurs.

## Seuil de réfutation, fixé avant les chiffres

Une graine est un succès **Paquet** si son rendement final est **≥ 0,90**, son
rendement dépasse celui de **Brassage d'au moins 0,20**, et ses deux tests causaux
sont **≤ 0,10**. Il faut **au moins 24 graines sur 32**. Un seuil raté donne
**NON** à cette approche et à ce budget, même s'il existe des paires adaptées.

Les témoins Bruit et Cristal doivent chacun atteindre un rendement ≥ 0,90 sur
**au plus 2 graines sur 32**. Sinon le dispositif est invalide, pas un succès.
Le selftest doit confirmer 100 % de réactions pour une paire positive écrite à
la main (SENSE_STIM/EMIT et SENSE_STIM/INC_A/EMIT), 0 % pour ses ablations, le
cristal, et l'absence d'effet sur l'ADN d'entrée et sur un monde extérieur.
Cette paire n'entre jamais dans les courses.

Les 32 graines sont toutes menées jusqu'à 512 générations. Il n'y a ni arrêt
au premier oui, ni nouveau mécanisme ou seuil après la mesure. Un échec honnête
reste un résultat. Les intervalles et comptes sont descriptifs ; les 8192
impulsions d'une graine ne sont pas 8192 réplications indépendantes.

## Reproduction

`run.sh` compile un ELF dans un répertoire `/tmp` neuf, lance `selftest`, puis
les 128 courses et produit du texte brut. Aucun Makefile partagé ni monde vivant
n'est utilisé. Les mesures détaillées seront dans `RESULTATS.md`, après le commit
de ce pré-enregistrement.
