# Rencontres, échanges et nurseries

Mission b129054c, base `9da2ed9`. Ce pilote ajoute trois physiques indépendantes
au terrarium, dans un **ELF neuf**. `src/`, `Params` (416 octets), `WorldConfig`,
`Creature` et l'ABI `LoiMonde` restent inchangés. Aucun `.so` n'est nécessaire.

| Profil | Changement physique |
|---|---|
| `control` | Terrarium historique ; relevé indépendant des contacts |
| `exchange` | Deux voisins vivants échangent un segment de soma et de germen, même au plafond |
| `nursery` | Deux parents voisins financent un œuf ; saison de conception, incubation et éclosion locale |
| `meeting` | Un génome portant capteur et STEP lit les marques et paie un pas vers elles |
| `all` | Les trois mécanismes ensemble |

Les règles sont posées par cette expérience. Leur présence et les résultats des
fondateurs conçus ne prouvent pas une émergence spontanée.

## Les trois leviers

Les échanges passent tous les 16 ticks : un partenaire au plus par individu,
repos de 64 ticks, coût de 3 unités pour chacun. Une paire sans différence ne
paie rien et ne compte pas comme échange réussi. Les adresses et longueurs
restent fixes ; au plus huit adresses contiguës sont échangées réciproquement.
Les rencontres sont toriques, sans restriction à la même espèce. La population,
les naissances et les décès ne sont pas modifiés par l'échange. Les génomes
réellement changés et les échanges faits au vrai plafond sont comptés séparément.

La nurserie ouvre la conception pendant les 160 premiers ticks d'une saison de
400, avec une tentative tous les 16 ticks. Deux parents distincts, à distance
torique de Chebyshev ≤ 1, paient chacun 24 unités d'énergie ; le nid consomme
deux unités de nourriture. L'œuf porte cet apport parental, un segment de chaque
parent, puis des substitutions de probabilité 0,005 par octet. Il mûrit en 24
ticks. Les parents observent un repos avant de reconcevoir.

Un quart du plafond est réservé au moteur différé pendant la VM ; cela n'ajoute
aucune place au vrai plafond. L'éclosion demande une case voisine du nid
entièrement vide. Un œuf bloqué attend, sans paiement supplémentaire ; 32 œufs
au plus par île. Aucun adulte n'est supprimé pour faire place. Les éclosions
sans décès dans l'île ce tick et les enfants différents des **deux parents avant
mutation** ont leurs propres compteurs. Au vrai plafond, les œufs attendent
encore : ce levier ne promet pas des naissances illimitées dans une place finie.

Les rendez-vous lisent une marque dans un rayon Manhattan de quatre cases,
puis orientent et paient un pas tous les quatre ticks selon une phase propre à
l'identité. Ils demandent la présence d'un capteur SENSE/LOAD et de STEP comme
opcodes, en sautant les immédiats. C'est une loi de reconnaissance du génome,
pas une preuve que ces instructions ont été exécutées ce tick. La VM reprend
ensuite son tour normal. Les traces propres viennent des EMIT non nuls et
expirent au bout de 24 ticks. Les marques des lois, si présentes dans **ce
nouvel hôte**, sont prioritaires. Aucune loi n'est déposée dans vie-tronc ou
vie-lecture ; `trace.so` n'est ni compilée ni copiée.

L'ABI existante ne donne accès ni aux créatures ni aux partenaires : une simple
loi `.so` ne pouvait pas ouvrir ces chemins de reproduction et de recombinaison.

## Construire sans relinker un vivant

Utiliser un identifiant de build qui n'a **jamais existé** :

```console
make experiments/rencontres-build/IDENTIFIANT_NEUF/garde
make experiments/rencontres-build/IDENTIFIANT_NEUF/test-garde
experiments/rencontres-build/IDENTIFIANT_NEUF/test-garde experiments/rencontres-build/IDENTIFIANT_NEUF/garde
```

Sur l'hôte où vivent les douze mondes, découvrir leurs PID par `--dir` et
enregistrer leurs exécutables **avant** de compiler la nouvelle physique :

```console
experiments/rencontres-build/IDENTIFIANT_NEUF/garde snapshot /tmp/octet-avant-IDENTIFIANT_NEUF
make experiments/rencontres-build/IDENTIFIANT_NEUF/terrarium
experiments/rencontres-build/IDENTIFIANT_NEUF/terrarium selftest
experiments/rencontres-build/IDENTIFIANT_NEUF/garde check /tmp/octet-avant-IDENTIFIANT_NEUF
```

`garde` refuse une liste incomplète, un doublon, un zombie ou un PPID autre que
1. Le contrôle compare PID, début du processus, inode/dev/taille/mtime/ctime
de `/proc/PID/exe` et le `--dir`. Le snapshot doit être neuf ; aucun fichier de
preuve n'est créé si les douze processus ne sont pas visibles. `--proc` sert
uniquement aux fixtures : leur succès ne vérifie pas les mondes de l'hôte.

Les règles de build refusent d'écraser un ELF existant, même avec `make -B`.
Elles compilent directement les sources vers le chemin neuf, sans toucher aux
objets partagés ni aux anciennes cibles. Ne pas utiliser `make` seul ou
`make test`. Le selftest du nouveau pilote ne lance pas le selftest historique
qui copie `exemple.so`.

## Faire naître un monde neuf

```console
mkdir experiments/vie-rencontres-IDENTIFIANT_NEUF
setsid -f nice -n 10 experiments/rencontres-build/IDENTIFIANT_NEUF/terrarium run --dir experiments/vie-rencontres-IDENTIFIANT_NEUF --profile all --ticks 0 --islands 1 --w 32 --h 16 --max-pop 160 --pop0 32 --regrow 120 --osmo 1 --stress 1 --migrate 0 --seed 101 > experiments/vie-rencontres-IDENTIFIANT_NEUF/boot.log 2>&1 < /dev/null
experiments/rencontres-build/IDENTIFIANT_NEUF/garde check /tmp/octet-avant-IDENTIFIANT_NEUF
```

Le pilote part de fondateurs et d'un sol vierge, sans `state.ck` importé. Il
refuse les noms des douze anciens répertoires, y compris via un lien symbolique.
Un verrou exclusif prévient deux pilotes dans le même répertoire. Il refuse les
imports de checkpoints historiques. Ne copier aucun répertoire vivant pour
préparer cet essai. Les premiers essais utilisent les seeds du dépôt ; une
autre expérience peut choisir `--founders random`, sans lancer soupe, fuzz ou
mix.

Migration et expansion sont désactivées dans cette première physique : les
cooldowns et la parenté utilisent des identités locales à chaque île. Le mode
copie réelle et les briques (`--forces 3`) sont refusés. Pour reprendre **son
propre** monde avec le même ELF, réutiliser `--dir` et un `--ticks` supérieur
ou nul ; le profil et les paramètres sont relus du checkpoint. Ne pas relancer
un processus encore vivant.

## Mesurer et vérifier

`series.tsv` et `societies.tsv` gardent leurs définitions historiques. Le critère
d'une société reste : au moins 1 000 paires et excès > 0,05 pendant trois fenêtres
de 200 ticks, événement sur front montant. Aucune rencontre géométrique n'est
rebaptisée société.

`rencontres.tsv` ajoute des compteurs **cumulatifs**. `contacts` compte chaque
paire géométrique i<j une fois par passage de 16 ticks ; une même paire peut
revenir au passage suivant. `exchanges` compte les couples dont l'ADN a changé,
`exchange_at_cap` le sous-ensemble effectué au plafond ; `hatched`,
`crossed_children`, `birth_no_death` distinguent éclosions, recombinaisons avant
mutation et éclosions sans mort du même tick. `reads`, `moves` et `move_energy`
mesurent la lecture et l'action guidée. Les refus de plafond et de place comptent
les œufs mûrs bloqués par tick, ceux de conception les fenêtres refusées.

Le checkpoint ajoute un trailer versionné `OADNREN1`, en dehors de `Params`,
au **même** fichier temporaire et au même `rename` que le monde. Œufs, marques,
cooldowns et générateurs aléatoires sont repris ensemble. Les fichiers produits
restent ignorés par Git. Les ombres historiques n'imitent pas ces nouvelles
interactions : leur écart ne doit pas servir de preuve de nouveauté sélectionnée.

Les selftests vérifient le plafond sans décès, les bords du tore, les coûts et
refus, les opcodes/immediats, les flags éteints et la reprise continue contre
checkpoint (monde, RNG, état des trois mécanismes et TSV identiques). Les onze
fixtures de garde vérifient aussi les refus sans création d'une fausse preuve.
Voir [RESULTATS.md](RESULTATS.md) pour les mesures et leurs limites.
