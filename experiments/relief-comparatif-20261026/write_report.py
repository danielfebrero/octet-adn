#!/usr/bin/env python3
"""Render the checked census summary as a French report (Markdown + HTML)."""
from pathlib import Path
import html,json,re
ROOT=Path(__file__).resolve().parent
x=json.loads((ROOT/'results/summary.json').read_text())
manifest=json.loads((ROOT/'results/run-manifest.json').read_text())
post=json.loads((ROOT/'results/postflight.json').read_text())
ARMS=('control','unlock','redistribute');names={'control':'Témoin','unlock':'Déblocage A','redistribute':'Redistribution B'}
def n(v):return f'{v:,}'.replace(',',' ')
def f(v,d=2):return f'{v:.{d}f}'.replace('.',',')
def pc(v,d=2):return f(100*v,d)+' %'
def table(headers,rows):return '\n'.join(['| '+' | '.join(headers)+' |','| '+' | '.join(['---']*len(headers))+' |']+['| '+' | '.join(map(str,r))+' |' for r in rows])+'\n'
def h(w,a,t='8192'):return x['windows'][w]['horizons'][t][a]
text='''# Relief : les caches, la survie et les descendants

Rapport final du 26 octobre 2026 — mission `b8e25e22-c4a1-4e91-814f-a8dd2ce11876`.

**Six essais réellement exécutés, deux états de départ, aucune modification du vivant.** Convertir localement environ un quart des caches en nourriture améliore fortement la survie courte, mais ralentit le remplacement sous plafond et concentre davantage la descendance. Égaliser les quantités de cache entre ceux qui y accèdent déjà a des effets agrégés beaucoup plus petits dans ce protocole. Cela distingue deux interventions concrètes ; cela ne prouve pas une loi universelle sur la matière ou la propriété.

## Résultats principaux, à 8 192 ticks

Chaque bras finit avec65 536vivants. Dans chaque trajectoire, naissances et décès cumulés sont égaux. Les **racines** sont les65 536individus présents au départ, chacun suivi séparément par tous les liens parent/enfant après le départ. Elles se distinguent des **lignées fondatrices**, bien plus anciennes.

'''
rows=[]
for wi,label in [('window1','F1'),('window2','F2')]:
 for a in ARMS:
  s=h(wi,a)
  rows.append([label,names[a],n(s['births']),('—' if a=='control' else pc(s['births_relative_delta_control'])),n(s['roots_with_extant_descendants']),str(s['lineages']),n(s['genotypes_exact'])])
text+=table(['Fenêtre','Bras','Naissances = décès','Écart témoin','Racines avec descendants vivants','Lignées fondatrices','Génomes exacts'],rows)
text+='''
Le déblocage réduit le renouvellement réalisé de34,26 % et34,73 %. Il réduit aussi les racines encore représentées dans la population finale de14,38 % et14,08 %. **Ce n'est pas une mesure de fécondité intrinsèque** : sous un plafond, une meilleure survie laisse moins de places vacantes. Les causes des décès et les refus de reproduction ne sont pas instrumentés ici.

La redistribution réduit les naissances de1,28 % et1,05 %. Son avantage apparent sur le nombre de racines représentées ne se retrouve pas :+32dans F1,−1dans F2. Il ne faut pas transformer cet écart en bénéfice général pour la descendance.

## Survivre, se reproduire, laisser une descendance

'''
rows=[]
for wi,label in [('window1','F1'),('window2','F2')]:
 for a in ARMS:
  short=h(wi,a,'600');mid=h(wi,a,'4096');last=h(wi,a)
  rows.append([label,names[a],pc(short['initial_survival_fraction']),pc(mid['initial_survival_fraction']),pc(last['fraction_roots_with_extant_descendants']),f(last['lineages_effective_shannon'],3)])
text+=table(['Fenêtre','Bras','Initiaux vivants à600','Initiaux vivants à4096','Initiaux avec descendance à8192','Lignées effectives à8192'],rows)
text+='''
À8192, **aucun individu initial ne survit dans aucun bras**. Chaque individu final appartient donc à la descendance prospective d'une racine initiale. La moyenne d'un descendant vivant par racine est mécaniquement65 536/65 536=1 ; elle ne mesure aucun progrès.

Dans les six bras, la médiane des descendants vivants par racine est zéro. Dans F1, les1 %de racines les plus représentées portent21,58 %des descendants témoins,22,78 %avec A et20,93 %avec B ; dans F2 :21,15 %,23,31 %,21,15 %. Le Gini de cette descendance passe de0,9076à0,9199avec A dans F1, de0,9062à0,9208dans F2 : la représentation des ancêtres devient plus concentrée.

Le nombre brut de lignées et leur répartition racontent des choses différentes. **A conserve26lignées contre25chez les témoins**, mais leur nombre effectif de Shannon baisse d'environ11 % et le nombre de génomes exacts baisse de7,38 % /6,40 %. Le nombre effectif résume l'équilibre des parts : plusieurs lignées minuscules comptent moins qu'autant de lignées équilibrées. Les génomes sont comparés sur leurs64octets, et les comptes par hash sont conservés séparément ; aucune collision observée à ces relevés.

Les sorties `results/windowN-lineage-results.tsv` contiennent pour chaque lignée : effectifs et parts, naissances/morts, survie de la cohorte initiale, enfants directs, descendants vivants, diversité et caches accessibles normalisés par tête. Les cohortes individuelles complètes restent dans `data/windowN/cohort-results.tsv`.

## Qui gagne des parts ?

Avec A, la lignée905perd2392individus par rapport au témoin dans F1 (−3,65points de population), et2299dans F2 (−3,51points). La3467gagne1910puis2229individus (+2,91 /+3,40points). Ces différences sont des réponses à cette intervention dans ces états, pas des aptitudes générales démontrées.

Avec B, la composition change beaucoup moins : distance totale entre les parts des lignées et celles du témoin1,25 % /0,98 %, contre10,12 % /11,35 %pour A. Cette distance vaut la moitié de la somme des écarts absolus de parts. Les gagnants/perdants de B sont souvent instables entre fenêtres ; les petites lignées doivent être lues avec leur dénominateur. Par exemple, la3921n'avait que7individus dans F1 : elle disparaît du témoin, reste sous B, alors que209(8individus initiaux) disparaît du témoin et de B mais reste sous A. Ces cas ne fondent pas à eux seuls une conclusion générale.

## Des interventions définies et vérifiées avant les essais

**Il n'existe pas de droit de propriété des caches dans ELF005.** Toute cache peut être retirée par un occupant ultérieur. `Cell.owner` conserve uniquement une trace du constructeur ; ce champ n'est jamais consulté pour autoriser un retrait. « Détenteur » signifie donc ici occupant initial d'une case ayant une cache positive.

- Témoin : fichier initial inchangé, octet pour octet.
- A : sur chaque case, `floor(cache/4)` est transféré du cache vers la nourriture de cette même case. Positions, génomes, inventaires, traces de construction et autres champs restent identiques. La somme locale nourriture+cache reste exacte et les caches positives restent positives.
- B : seules les cases initialement occupées à cache positive participent. Dans chacune des256phases de décomposition (`position modulo256`), les quantités sont égalisées au quotient entier, puis le reste est attribué par index de case croissant. Quantité globale, quantité sous occupants, support positif et totaux par phase sont conservés. Les cases inoccupées et les occupants sans cache positive ne reçoivent rien.

'''
rows=[]
for wi,label in [('window1','F1'),('window2','F2')]:
 i=x['windows'][wi]['initial'];a=i['unlock']['transform'];b=i['redistribute']
 rows.append([label,n(a['released_cache_to_food']),pc(a['released_cache_to_food']/i['control']['cache_total'],3),n(b['units_reallocated']),n(i['control']['cache_occupied']),f(i['control']['cache_occupied_gini'],4)+' → '+f(b['cache_occupied_gini'],4),f(i['control']['lineage_cache_percapita_cv_population_weighted'],4)+' → '+f(b['lineage_cache_percapita_cv_population_weighted'],4)])
text+=table(['Fenêtre','A : unités cache→nourriture','Part des caches','B : unités réaffectées','Cache sous occupants, inchangé B','Gini par occupant, B','Écart des moyennes par lignée, B¹'],rows)
text+='''
¹Coefficient de variation des moyennes de cache par tête de chaque lignée, pondéré par son effectif. Le Gini par occupant inclut aussi les occupants sans cache. Le nombre de détenteurs positifs reste60543dans F1 et60004dans F2. Le Gini non pondéré des moyennes par lignée baisse également :0,1627→0,0593et0,1516→0,0423. L'égalisation a donc effectivement modifié la concentration ; elle ne s'est pas limitée à réétiqueter des champs inertes.

**Limite d'interprétation de A :** la nourriture produit de l'énergie lorsqu'elle est mangée, devient perceptible par les capteurs, se décompose au rythme1/64plutôt que1/256et peut modifier la production solaire locale au seuil96. Le stock de matière et l'énergie ne sont pas la même variable ; l'énergie du modèle est ouverte. A teste une conversion locale en matière comestible, avec ces conséquences, pas une injection de matière ni un changement pur de quantité totale.

**Limite de B :** égalisation ponctuelle entre les accès positifs existants, pas ouverture de l'accès à tous ni règle permanente. Dans F1, seulement8,97millions sur66,98millions d'unités de cache participent au pool sous occupants. Le Gini par occupant de B remonte à0,5772 /0,5748à8192(témoins0,6415 /0,6366) avec les déplacements et échanges. L'égalité initiale ne persiste pas intégralement.

Les contrastes sont A−témoin et B−témoin. Les doses et mécanismes diffèrent ; leur différence n'est pas une décomposition universelle disponibilité/propriété. Aucun effet combiné A×B n'est revendiqué et aucun bras combiné n'a été ajouté.

## Matière, interactions et plafond

Les76840404unités de matière sont conservées exactement à chaque export et contrôle du moteur. À8192, les réserves physiques diffèrent :

'''
rows=[]
for wi,label in [('window1','F1'),('window2','F2')]:
 for a in ARMS:
  s=h(wi,a);r=s['resources']
  rows.append([label,names[a],n(r['cache']),n(r['food']),n(r['body']+r['store']),n(r['energy']),n(s['gifts']),n(s['attacks']),n(s['parasites'])])
text+=table(['Fenêtre','Bras','Caches','Nourriture','Corps+inventaires','Énergie²','Dons','Attaques','Parasitismes'],rows)
text+='''
²L'énergie est donnée séparément et n'entre pas dans le bilan de matière. Minéral, détritus et structures sont également détaillés dans les JSON de résultats. Les compteurs de dons/attaques/parasitisme ci-dessus sont exhaustifs. Les lignes d'interaction des journaux sont échantillonnées à1/256 : elles restent identifiées comme telles. Ni société, ni raisonnement, ni réciprocité adaptative ne sont déduits de ces dons.

La population ne suffit pas à décrire le renouvellement : les six populations finales sont égales mais les flux diffèrent fortement. Les recensements ont été conservés sur la grille prévue tous les100ticks. Un raffinement secondaire, défini avant analyse des résultats, reconstruit en plus exactement la population en fin de chaque tick à partir de toutes les naissances/morts :

'''
rows=[]
for wi,label in [('window1','F1'),('window2','F2')]:
 for a in ARMS:
  s=h(wi,a);p=s['population_tick_end'];grid=s['population_grid100']
  rows.append([label,names[a],n(p['minimum']),str(p['ticks_below_capacity'])+'/8192',f(p['mean'],4)])
text+=table(['Fenêtre','Bras','Minimum en fin de tick','Ticks sous plafond','Moyenne fin de tick'],rows)
text+='''
Ce recensement ne mesure pas le nombre de places libres à l'intérieur d'un tick. Dans B/F1, les échantillons tous les100ticks ne voient aucun déficit alors que50fins de tick sont sous plafond : les deux résolutions sont distinguées, sans trace intra-tick illimitée.

## Ce qui est confirmé, réfuté, non tranché

- **Confirmé dans les deux fenêtres :** A améliore la survie courte et change fortement la composition de la population, avec moins de renouvellement réalisé et davantage de concentration des descendants à8192. B réduit réellement la concentration initiale à quantité de cache accessible conservée.
- **Réfuté sur ces états et horizons :** la prédiction « convertir ce quart des caches en nourriture augmente le renouvellement et élargit la représentation des ancêtres à deux saisons ». Les données montrent la direction opposée. Cela ne réfute pas l'utilité des ressources pour survivre, bien démontrée à600ticks.
- **Non tranché :** un avantage général de B pour la descendance/diversité ; un rôle dominant de toute la concentration de matière ; les effets d'un accès donné aux occupants sans cache, d'une égalisation permanente, d'autres doses, d'autres phases climatiques ou d'horizons plus longs ; les interactions entre A et B ; le mécanisme précis de limitation de la reproduction.

Il n'y a ni p-value ni intervalle de confiance fondé sur des individus supposés indépendants. Les chiffres sont des recensements exacts de trajectoires finies. Les deux départs sont séparés de4200ticks, environ une saison plus104ticks ; leurs horizons se recouvrent et ils proviennent du même monde. **Ce sont deux fenêtres de robustesse temporelle dépendantes, pas deux réplications indépendantes.** Le rejeu déterministe de vérification n'est pas une réplication supplémentaire. Les RNG divergent normalement entre bras après intervention à cause des branches d'exécution ; seul le RNG initial est exigé identique.

## Provenance et témoin déterministe

Les checkpoints ont été copiés par un seul descripteur de fichier ouvert. Le moteur sauvegarde sur un nouvel inode puis renomme atomiquement : une copie ne mélange donc pas deux générations. Aucun arrêt, pause ni verrou du vivant n'a été nécessaire.

'''
for wi,label in [('window1','F1'),('window2','F2')]:
 c=x['windows'][wi]['capture'];initial=x['windows'][wi]['initial']['control']
 cap=c['inspect'];rng=cap['rng'] if isinstance(cap,dict) else 'dc1a663677e1a60c'
 text+=f'''**{label}** — tick{initial['tick']}, RNG `{rng}`, capture `{c['captured_at_utc']}`.

SHA256 état : `{c['sha256']}`.

Commit au relevé : `{c['commit']}`.

'''
text+='''ELF005 original, utilisé pour tous les ticks expérimentaux :

`c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480`

Source d'origine, restée inchangée :

`10ac2a592745b2a83bd657f01961a01a3ff8ee7b8709ce0e4310f74dd3ac080c`

Le témoin F1 a été rejoué de737400à738000avec l'ELF005 exact. Son fichier entier est identique au checkpoint vivant effectivement capturé à738000 :SHA256 `a0bc658db6efa2ec191d939b5e981b26612eb5321bc8a5929fe82b701ada5974`. La preuve antérieure693000–693600et ses11069naissances/morts,118870dons et59159bénéficiaires agissant ensuite sont conservées dans `../relief-observatoire/observation-20261026-intratick.txt` ; elles n'ont pas été confondues avec les résultats nouveaux.

Le protocole a été figé au commit `4df4a5d`, poussé avant les simulations comparatives. Les outils initiaux sont au commit `ebfcedf`, les cohortes/tests au commit `7e01010` ; les compléments analytiques et résultats accompagnent ce rapport. Les seules simulations sont celles de l'ELF005 ; l'instrumentation hors moteur ne consomme pas son RNG.

## Contrôles, charge et stockage

Les tests `check.py` ont réussi : transformations répétées identiques, vérification indépendante des champs binaires permis, conservation et règle d'arrondi/redistribution, checksum corrompu rejeté, refus de destination existante et du vivant même via symlink. Tous les octets des agents, de la configuration et du RNG initial sont identiques ; seuls cache/nourriture autorisés et checksum diffèrent. Sauvegarde/rechargement par le chargeur d'origine contrôlés.

Le selftest analytique vérifie les descendants après mort de la racine, plusieurs générations, la séparation des liens familiaux déjà présents au départ, la cohérence des flux, les dons échantillonnés et l'extinction absorbante sans fondateurs. Une relecture indépendante a recoupé les formules, des différences binaires et la généalogie effective du témoin. Les flux exhaustifs de chaque lignée et chaque état vivant reconstruit concordent avec les exports.

'''
text+=f'''Au total : **{n(manifest['simulated_ticks'])}ticks simulés**, dont49152pour les six bras et600pour la vérification technique, sous la limite60000. Simulations séquentielles plafonnées à0,75CPU et256Mio, sans réseau, avec seulement le dossier de copie en écriture. Durée au mur du pilote :{f(manifest['wall_seconds'],1)}secondes ; somme des appels comparatifs :{f(sum(r['wall_seconds'] for r in manifest['runs']),1)}secondes. Pas de journal intra-tick ajouté.

Budget de fichiers :2Gio. Le manifeste post-analyse donne le volume effectif et toutes les empreintes ; le volume au terme du pilote était{n(manifest['data_bytes'])}octets ({f(manifest['data_bytes']/1024**3,3)}Gio). Les sorties de cohorte et bilans ultérieurs sont incluses dans l'inventaire final :1 756 181 930octets (1,636Gio),221fichiers bruts. Aucun conteneur d'expérience permanent ne subsiste.

`histoire/STOCKAGE.txt` et la stratégie de stockage ont été lus avant les essais. Au relevé préflight :318jours projetés,statutok. Dernier relevé de stockage disponible au postflight :312jours projetés,0,838Go/jour,histoire1,861Go,statutok ;243Gio libres mesurés aussi pardf. **`histoire/stockage.log` était absent** : les segments n'avaient pas encore24heures et aucune recompression éligible n'était attendue ; cela ne vaut pas un journal vide certifié sans erreur. Aucune action de stockage niveau3, aucune suppression d'histoire.

Le vivant a progressé du tick{manifest['live_before']['tick']}au tick{manifest['live_after']['tick']}pendant le pilote ; au contrôle supplémentaire16:45:59Z, tick745800,population65536,matière76840404. Même conteneur etPID647691,StartedAt25/10/2026 19:56:08Z,RestartCount0,aucunOOM,ELF et source exacts. Aucun ralentissement anormal observé aux relevés, sans prétendre à un benchmark de latence complet. Aucun ancien monde ni E1 n'a été relancé, aucun fondateur recréé, aucune loi déployée ni fusionnée dans le vivant.

## Prochaine étape recommandée

Conserver Relief vivant tel quel. Sur copies, compter de façon bornée les causes de mort (âge/famine/attaque) et les refus de reproduction (plafond, voisin occupé, énergie, corps, réserve). Vérifier d'abord que cette observation passive reproduit l'ELF005 octet pour octet. Puis tester une dose plus petite de conversion, par exemple1/16, aux mêmes horizons. Cela permettrait de distinguer l'amélioration de survie, la raréfaction des places et un éventuel changement de capacité reproductive. L'étude actuelle ne justifie aucun changement de loi dans le vivant.

## Lire et reproduire les preuves

Racine locale : `/workspace/produits/octet-adn/experiments/relief-comparatif-20261026/`.

- `PROTOCOLE.md`, `preregistration.json` : décisions avant les runs et empreinte du protocole.
- `results/summary.json`, `results/windowN-analysis.json` : chiffres et définitions ; `windowN-comparison.tsv` : contrastes ; `windowN-lineage-results.tsv` : toutes les lignées, dénominateurs et ressources normalisées.
- `results/run-manifest.json` : commandes Docker exactes, durée, états et empreintes par segment ; `replay-verification.json`, `check-results.json`, `postflight.json` : contrôles.
- `results/evidence-manifest.json` : inventaire et SHA256 de tous les fichiers bruts retenus, états, exports, événements et cohortes.
- `data/windowN/base/state.ck` : captures initiales ; dossiers `control/`, `unlock/`, `redistribute/` : checkpoints initiaux et600/4096/8192, `transformation.json` et `transformation.cells.tsv`, événements et séries.
- `README.md` : commandes de compilation, tests, analyse et reproduction. `reproduce.py` repart des deux captures figées, sur un nouvel output, sans lire le vivant ni invoquerinit.

Les gros artefacts restent locaux dansdata, ignorés pargit ; les sources, contrôles, manifestes, agrégats et rapport sont versionnés et poussés. La reproduction exige ces captures conservées ; les manifestes seuls ne permettraient pas de reconstituer un état disparu.
'''
# Restore readable word spacing in prose, keeping identifiers and paths literal.
chunks=text.split('`')
for k in range(0,len(chunks),2):
 chunks[k]=re.sub(r'(?<=[a-zà-ÿ])(?=\d)|(?<=\d)(?=[a-zà-ÿ])',' ',chunks[k])
 chunks[k]=re.sub(r'(?<=\d)(?=(?:Gio|Go|Mio|CPU|PID))',' ',chunks[k])
 chunks[k]=re.sub(r'%(?=\S)','% ',chunks[k])
 chunks[k]=re.sub(r'\bÀ(?=\d)','À ',chunks[k])
 chunks[k]=re.sub(r'(?<=\d)(?=(?:individus|vivants|lignées|naissances|ticks|unités|dans|puis|millions|heures|secondes|octets|avec|contre|chez|points|à|et|de|par|est|plutôt))',' ',chunks[k])
text='`'.join(chunks)
(ROOT/'RAPPORT.md').write_text(text)
# Minimal renderer for this known report format; all raw text HTML-escaped.
def inline(s):
 s=html.escape(s);s=re.sub(r'`([^`]+)`',r'<code>\1</code>',s);s=re.sub(r'\*\*([^*]+)\*\*',r'<strong>\1</strong>',s);return s
lines=text.splitlines();parts=[];i=0
while i<len(lines):
 l=lines[i]
 if not l.strip():i+=1;continue
 if l.startswith('#'):
  level=len(l)-len(l.lstrip('#'));parts.append(f'<h{level}>'+inline(l[level:].strip())+f'</h{level}>');i+=1
 elif l.startswith('|'):
  block=[]
  while i<len(lines) and lines[i].startswith('|'):block.append(lines[i]);i+=1
  parts.append('<div class="table"><table>')
  for j,row in enumerate(block):
   if j==1:continue
   tag='th' if j==0 else 'td';parts.append('<tr>'+''.join(f'<{tag}>'+inline(c.strip())+f'</{tag}>' for c in row.strip('|').split('|'))+'</tr>')
  parts.append('</table></div>')
 elif l.startswith('- '):
  parts.append('<ul>')
  while i<len(lines) and lines[i].startswith('- '):parts.append('<li>'+inline(lines[i][2:])+'</li>');i+=1
  parts.append('</ul>')
 else:
  para=[l];i+=1
  while i<len(lines) and lines[i].strip() and not lines[i].startswith(('#','|','- ')):para.append(lines[i]);i+=1
  parts.append('<p>'+inline(' '.join(para))+'</p>')
page='<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Relief — expérience comparative</title><style>body{font:17px/1.55 system-ui,sans-serif;max-width:1000px;margin:auto;padding:22px;color:#152d36;background:#fafcfc}h1,h2{line-height:1.2;color:#164553}h2{margin-top:2em}code{font-size:.84em;overflow-wrap:anywhere}p,li{overflow-wrap:anywhere}.table{overflow-x:auto}table{border-collapse:collapse;font-size:.87em;width:100%}th,td{text-align:left;padding:8px;border-bottom:1px solid #cbd8db}th{background:#e5eff0}tr:nth-child(even){background:#f0f5f5}strong{color:#123e49}</style><main>'+''.join(parts)+'</main></html>'
(ROOT/'RAPPORT.html').write_text(page)
print('reports written',len(text),len(page))
