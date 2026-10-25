# Héritage du milieu : transmettre un sol sans transmettre un corps

## Pré-enregistrement, ordre 0996a2cd

**L'approche.** Un organisme peut transmettre une niche utile par le champ de
nourriture qu'il laisse, même quand aucun de ses corps, registres, génomes de
vie ou générateurs aléatoires n'est transmis à la génération suivante.

**Prédiction.** Après le passage de fourrageurs, des descendants frais de même
génome réussissent mieux dans leur milieu conditionné que dans des milieux de
même quantité de nourriture où ce legs spatial a été supprimé.

Les marques de `causales-ligne` agissent sur le stimulus du prochain tick, chez
des contemporains. Ici le test coupe complètement les deux générations et ne
transmet que `food[][]`. Ce n'est ni un détecteur de période, ni une variante de
rencontres, ni une nouvelle loi de copie. Il sonde un canal d'hérédité hors ADN.
Le résultat ne prouvera aucune apparition spontanée depuis le bruit.

## Dispositif figé avant les comptes

Le programme C séparé lie la VM et le monde existants, sans les modifier.
Chaque monde est neuf, en mémoire : 32 × 16, densité de nourriture 0,50,
repousse 0, plafond 64, énergie initiale 160, seuil SPLIT 70. Quatre fondateurs
de la seed `forager` passent **96 ticks donneurs**. Les morts naturelles du
moteur rendent quatre unités de nourriture ; la consommation et ces dépôts
peuvent donc réellement changer la structure du champ. Aucun hook de culture,
de récompense ou de déplacement de nourriture n'est ajouté.

Substitution, duplication et crossover sont à zéro ; `germline=1` copie le
génome de naissance lors de SPLIT. La réécriture du soma reste possible pendant
la vie. Cela maintient la génétique à l'identique pour isoler l'hérédité du
milieu. Les récepteurs sont quatre créatures nouvellement construites, aux
positions et orientations **initiales** des donneurs, avec la seed initiale,
registres et compteurs nuls. Le champ final des donneurs est photographié puis
le monde donneur est libéré. Trois nouveaux mondes récepteurs passent **160
ticks** :

| bras | champ transmis |
|---|---|
| H, héritage | le champ final des donneurs, à l'identique |
| P, permutation | Fisher–Yates de toutes les cases de H : même histogramme et même somme, géométrie détruite |
| V, sans passage | champ initial des donneurs, ajusté à la somme de H par retrait ou ajout d'unités sur des cases admissibles tirées au hasard, sans consulter les positions des créatures |

V évite de confondre l'organisation du sol avec sa richesse globale. Un ajout
éventuel est borné à 14 par case, comme dans le moteur. Les trois mondes ont
le même état RNG de départ, indépendant du monde donneur et des opérations de
permutation/ajustement. Leurs trajectoires RNG peuvent diverger avec les
naissances ; aucune égalité de trajectoire n'est revendiquée.

Les observables sont le **nombre de naissances après les quatre fondateurs**
pendant les 160 ticks, et la somme des énergies vivantes intégrée sur les
ticks **81 à 160**. L'intégrale vaut zéro après extinction. Les populations
finales, morts, nourriture et hashes sont des diagnostics, pas d'autres seuils.

## Barre réfutable

Une graine passe si H a simultanément :

- une biomasse intégrée strictement positive, au moins **1,10 fois** celle de P
  **et** de V ;
- au moins une naissance, et au moins **1,20 fois** les naissances de P **et** de V.

Les comparaisons sont faites par produits entiers (10 H ≥ 11 témoin et
5 H ≥ 6 témoin), sans arrondir les ratios. Un témoin nul n'autorise donc pas un
H nul à passer.

| groupe | graines | résultat exigé |
|---|---:|---|
| forager | 1 à 32, base monde `0x45434f000000` + graine | **au moins 24/32** passent |
| bruit | 1 à 16, base monde `0x425255000000` + graine ; un génome uniforme de 256 octets par graine, RNG indépendant | **au plus 2/16** passent |
| cristal | 1 à 16, base monde `0x435259000000` + graine ; 256 octets `0x11` (`SENSE_EN`, sans EAT ni SPLIT) | **0/16** passe |

**Oui seulement si les trois lignes passent.** Tout seuil raté réfute la
prédiction dans ce dispositif à ce volume. Les 32 graines suffisent à distinguer
une réussite fréquente demandée (75 %) de quelques cas isolés. Les bras bruit et
cristal sont des contrôles de spécificité de la conclusion, pas une définition
générale du vivant. Pas de second mécanisme après les mesures.

Avant les comptes, `selftest` doit vérifier la conservation des quantités,
l'histogramme de P, l'appariement des créatures réceptrices, l'absence d'état
donneur transmis, le déterminisme, le champ constant invariant sous permutation,
et les frontières exactes du critère de décision. Un test cassé est corrigé
avant de mesurer ; aucune mesure de groupe n'est utilisée pour régler la sonde.

## Reproduction

Depuis la racine du dépôt, le script construit un ELF dans un chemin `/tmp`
nouveau et exécute d'abord les selftests. Il écrit les comptes dans un autre
répertoire neuf ; ni ELF ni fichiers de run ne sont versionnés.

```sh
sh experiments/heritage-milieu/mesure.sh
```

Les seuils et constantes ci-dessus doivent être committés avant l'appel `run`.
