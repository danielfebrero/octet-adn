# gene-transplant-matrix : appeler des gènes par matrice de transplantation

Point de départ : `experiments/general-transfer/` montre qu'**un octet de la lignée 3
(0x79:17>cf) débloque la lignée 2**. Cette expérience généralise ce fait isolé en une
méthode statistique : on prend un motif d'octets chez un génome qui réussit, on le greffe
sur **beaucoup de fonds** et on regarde où il marche.

## Méthode

1. **Panel de fonds** (`panel/`, 40 génomes + la seed `seeds/general.adn`). Il y a 8 lignées
   indépendantes, `./octet-adn --labs general --cycles 60 --seed S --no-reload`
   avec S = 0..7, moteur d3a6e92. Dans chaque checkpoint, on dédoublonne pool + best (longueur 172,
   ≠ seed) puis on garde **5 représentants par lignée** par sélection farthest-point
   (distance de Hamming, le premier étant le plus éloigné de la seed). Le panel fait 41 fonds,
   dont 23 passants (m2 ou m3 ≥ 0,95).
2. **Candidats.** Dans chaque génome passant, on prend toutes les fenêtres contiguës k=1..4
   dont les deux octets de bord diffèrent de la seed. Chaque candidat est étiqueté par la
   mémoire du donneur (m2/m3) et dédoublonné sur (pos, k, octets, t). On obtient 438 motifs
   (k1 278, k2 61, k3 41, k4 58).
3. **Matrice effet[motif, fond].** On copie les octets du motif à leur position sur le fond,
   puis `gn_check` mesure 40 tâches tenues à l'écart. La cellule vaut Δm_t (t = mémoire du donneur).
   Le fond est *inéligible* si m_t ≥ 0,95 déjà, et *porteur* (non mesuré) s'il a déjà le motif.
   Une cellule est **positive** si Δm_t ≥ +0,05. Au total, 24 551 cellules sont mesurées.
4. **Contrôle nul apparié.** Chaque motif a un jumeau de même position et de même longueur,
   avec des allèles aléatoires (≠ seed). Il passe par le même pipeline et le même classement.
5. **Contrôle positif.** On regreffe 0x79:17>cf sur `general-transfer/seed2-pool3` :
   m2 passe de 0,530 à 0,987 (Δ +0,457, code 2). Le fait fondateur est donc retrouvé.

## Définition opérationnelle

- **GÈNE PORTABLE** : positif dans ≥ 3 lignées distinctes (graines), **ET** autonome
  (positif seul sur la seed, Δ ≥ +0,05), **ET** Δ médian > 0 sur les fonds éligibles.
- *portable non autonome* : ≥ 3 lignées, mais sans effet sur la seed.
- *deux lignées* : exactement 2 lignées.
- **HACK** : positif dans une seule lignée.
- *inerte* : aucun fond positif.
- **Familles** : union-find sur l'indice de Jaccard (≥ 0,5) des ensembles de fonds positifs,
  à mémoire égale.
- Colonnes de profil : `Δautre+` donne l'effet médian sur l'autre mémoire là où le motif est
  positif (antagonisme). `ctx+` compte les fonds positifs qui passent déjà l'autre mémoire
  (≥ 0,85).

## Résultats (voir `results.txt` ; matrice complète `matrix.tsv` : NA / P / Δ)

| classe | réels | nuls |
|---|---|---|
| GÈNE PORTABLE | **0** | 0 |
| portable non autonome | 14 | 1 |
| deux lignées | 10 | 9 |
| HACK (1 lignée) | 49 | 50 |
| inerte | 365 | 378 |

**Verdict : 0 gène portable autonome.**

- Les **hacks sont au niveau du hasard** (49 réels contre 50 nuls). Un effet confiné à une
  seule lignée ne se distingue pas d'un allèle aléatoire à la même position.
- En revanche, la classe **portable (≥ 3 lignées)** est nettement au-dessus du nul (14 contre 1).
  Elle se ramène à 3 loci k=1 et à une famille de fenêtres k=2..4 autour de 0x75..0x8c (famille m2,
  13 motifs, noyau de 18 fonds dans 8 lignées).

Top 3 (k=1) :

1. `m2@78:14>13` : positif dans **8/8 lignées**, 19/25 fonds (6 porteurs), Δ médian **+0,613**,
   solo −0,035, 14 sauvetages (le fond devient passant m2). Δautre+ vaut **−0,623**, et 16/19
   fonds positifs sont des passants m3. Le nul apparié a un Δ médian de −0,018.
2. `m2@89:1f>14` : 8/8 lignées, 19/31 fonds, Δ médian +0,488, solo −0,131, 13 sauvetages,
   Δautre+ −0,623. Il y a de l'épistasie de signe : −0,73 et −0,70 sur les fonds quasi-m2 s5-p11 et s5-p15.
3. `m3@72:5b>08` : 3/8 lignées, 5/25 fonds, Δ médian 0,000, solo −0,009. C'est faible, au niveau
   de l'unique faux positif nul de la classe.

**Lecture.** Les deux loci forts ne sont pas des gènes autonomes : ce sont des
**interrupteurs alléliques m3→m2**. Ils n'agissent que sur des génomes qui portent déjà
l'échafaudage « registre décalé » de la mémoire-3, et ils échangent m3 contre m2 :
m3 s'effondre quand m2 apparaît. En 0x78, la seed a EMIT (0x14) et les passants m2 ont
NEURAL (0x13) ; en 0x89, les passants m2 ont EMIT. Un seul octet choisit donc quelle profondeur
du registre est lue ou émise. m2 et m3 se comportent comme deux allèles d'un même locus, ce qui
explique structurellement le négatif conjoint m2∧m3. Le fait fondateur (0x79:cf) est classé
inerte dans la matrice, parce que presque tous les fonds éligibles le portent déjà ; le contrôle
positif le retrouve bien.

Limites : seuil fixe (Δ ≥ 0,05) ; fenêtres contiguës seulement, même position, pas de paires
non contiguës ; un seul lab (general) ; 60 cycles par lignée.

## Reproduire

```sh
make octet-adn experiments/gene_transplant
# panel (déjà versionné dans panel/ ; à régénérer seulement pour l'étendre)
for S in 0 1 2 3 4 5 6 7; do
  ./octet-adn --labs general --cycles 60 --seed $S --no-reload --data-dir /tmp/gtm/s$S
done
./experiments/gene_transplant panel /tmp/gtm/panel /tmp/gtm/s{0..7}/general/checkpoints/latest.ck
# matrice (~3 min avec 7 jobs ; OADN_JOBS règle le parallélisme)
./experiments/gene_transplant matrix experiments/gene-transplant-matrix/panel /tmp/matrix.tsv > /tmp/results.txt
diff /tmp/results.txt experiments/gene-transplant-matrix/results.txt
cmp /tmp/matrix.tsv experiments/gene-transplant-matrix/matrix.tsv
```

Déterminisme : deux exécutions, avec OADN_JOBS=7 et OADN_JOBS=3, donnent une `matrix.tsv`
identique à l'octet près et le même `results.txt`.
