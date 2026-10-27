"""Registre traçable. Les statuts décrivent une capacité, jamais une preuve d'émergence.
Les sources historiques sont lues seulement; generate ne démarre aucun ancien monde.
"""
from pathlib import Path
import hashlib
import random
import re

ROOT = Path(__file__).resolve().parents[2]

# Every native instruction keeps its original meaning and numeric value.
CORE = ['NOP','INC_A','DEC_A','INC_B','DEC_B','CLR_A','STORE','LOAD','JMP','JZ','SENSE','TURN','STEP','EAT','SPLIT','YIELD','SENSE_NB','SENSE_EN','SENSE_STIM','NEURAL','EMIT','MOV_BA','MOV_AB','ADD','SUB','XOR','ATTACK','JNZ','LOADI','SENSE_HIST','LOOP','AND']
RELIEF = ['manger','avancer','orienter','croître','reproduire','construire','défaire','déposer','reprendre','attaquer','parasiter','donner','émettre','percevoir','conditionner','attendre']
TRANSMISSION = ['NOP','IMM','LOAD','STORE','NEXT','PREV','ZERO','JUMP','JZERO','EMIT','EAT','SPLIT','RESTORE','MAIL','RANDOM','XOR','ADD','SWAP','SOURCE','BOOK','REQUEST','OUTCLEAR','SETLO','SETHI','GETLO','GETHI','INC','DEC','SHAREDSTORE','CODESTORE','SENSE','YIELD']

CORE_EFFECTS = [
 'Intron gratuit; la recherche est bornée à la longueur du génome.',
 'A←A+1 modulo256.', 'A←A−1 modulo256.', 'B←B+1 modulo256.', 'B←B−1 modulo256.', 'A←0.',
 'Lit imm8; écrit A dans soma[imm8 modulo longueur], ou fenêtre de réparation si active et imm8≥224. Marque le codon comme réécrit.',
 'Lit imm8; charge soma[imm8 modulo longueur] dans A, ou la fenêtre de réparation si active.',
 'Lit imm8 puis avance le pointeur de cet offset, modulo longueur.',
 'Lit imm8; avance de cet offset si A=0.',
 'Charge dans A la nourriture devant soi.', 'Orientation←A modulo4.',
 'Tente un pas dans son orientation selon la grille et le coût natif.',
 'Consomme la nourriture locale selon les règles énergétiques natives.',
 'Tente une division selon énergie, plafond, place et règles germen/soma natives.',
 'Termine le budget du tick.',
 'A←min(255,nombre de voisins).', 'A←min(255,énergie divisée par8).',
 'A←stimulus extérieur courant sur8bits.',
 'Lit adresse imm8; neurone4entrées nourriture/voisins/énergie/stimulus, poids int8 dans génome et biais; sortie saturée0..255.',
 'Première émission du tick: publie A; émissions suivantes du même tick ignorées.',
 'B←A.', 'A←B.', 'A←A+B modulo256.', 'A←A−B modulo256.', 'A←A XOR B.',
 'Tente prédation selon espèce, proximité, énergie et paramètres natifs.',
 'Lit imm8; avance le pointeur de cet offset si A≠0.',
 'A←octet immédiat suivant.', 'Lit imm8; A←valeur de l’historique à cet indice.',
 'Pointeur←0.', 'Lit imm8; A←A AND imm8.'
]
RELIEF_EFFECTS = [
 'Coût1énergie;1..4nourriture locale→réserve et24énergie/unité, avec plafonds.',
 'Coût2+corps/16+construction_destination/8; pas vers case libre sans roche ni pente≥180.',
 'Coût1; orientation tourne de1+(argument modulo3).',
 'Coût4énergie;1réserve→1corps, corps<256.',
 'Corps≥12,réserve≥8,énergie≥100,voisin libre; coût20 puis partage corps/réserve/énergie et héritage natif.',
 'Coût6;1réserve→1construction locale, plafond256; inscrit marque de lignée.',
 'Coût4;1construction→1réserve, plafond réserve4096.',
 'Coût2;1réserve→1cache commune, plafond65536.',
 'Coût2;1cache commune→1réserve, plafond4096.',
 'Coût8; si force corps+(génome[63]&15)>corps_cible+abri/2, prélève réserve puis corps de la cible, avec plafonds.',
 'Coût4; si corps≤corps voisin, siphonne au plus8énergie, borné par disponibilité et plafond.',
 'Coût2 plus don4énergie et1réserve à voisin; conditions de réserve et plafonds, aucun bonus.',
 'Coût3; signal local augmente de16+4×argument, plafond4095.',
 'Mémoire[argument&7]←capteur: nourriture×8,énergie/4,réserve×8,nourriture devant×8,occupation devant,humidité/4,signal/8 ou construction×8; saturation255.',
 'Si mémoire[argument&7]<(64 si argument<8 sinon192), saute un octet; au plus8instructions décodées par tick.',
 'Aucun acte; entretien normal.'
]
TRANSMISSION_EFFECTS = [
 'Attend une instruction, coût énergétique normal.', 'A←octet immédiat du code.',
 'A←octet du support sélectionné: RAM, code, artefact partagé ou livre, à l’adresse P.',
 'RAM[P modulo4096]←A.', 'P←P+1 modulo65536.', 'P←P−1 modulo65536.', 'P←0.',
 'Lit adresse little-endian16bits et saute modulo512.',
 'Lit adresse little-endian16bits et saute modulo512 si A=0.',
 'Ajoute A à la sortie si elle contient moins de4096octets.',
 'Alimentation abstraite +native_eat_gain énergie plafonnée4096; termine le tick.',
 'Si énergie≥3000 et place libre: coût80, partage énergie et copie code/RAM; erreurs possibles selon native_mutation_divisor.',
 'Compare sortie explicite à la cible: même longueur et tous octets identiques. Bonus min(1024+2×longueur,plafond4096) après coût16 et délai512ticks.',
 'Poste sortie si limites et cooldown satisfaits; coût32+longueur/32, délai individuel2048 et global512ticks; provenance enregistrée.',
 'A←octet du RNG natif déterministe.', 'A←A XOR B.', 'A←A+B modulo256.', 'Échange A et B.',
 'Support←A AND3: RAM/code/partagé/bibliothèque.', 'Case livre/artefact←A modulo8.',
 'Demande titre ASCII1..96octets dans sortie; coût64, délai4096ticks; écrit requête du broker, sans réseau arbitraire.',
 'Vide la sortie sans effacer les mémoires.', 'P bas←A, conserve octet haut.', 'P haut←A, conserve octet bas.',
 'A←octet bas de P.', 'A←octet haut de P.', 'A←A+1 modulo256.', 'A←A−1 modulo256.',
 'Coût supplémentaire2; artefact partagé[case][P modulo4096]←A et marque propriétaire.',
 'Coût supplémentaire2; code[P modulo512]←A.', 'A←énergie/16 sur8bits.', 'Termine le tick.'
]
EFFECTS={'legacy':CORE_EFFECTS,'relief':RELIEF_EFFECTS,'transmission':TRANSMISSION_EFFECTS}

# Historical families are explicitly distinct: e.g. tronc osmosis is not the full soup.
FAMILIES = [
 ('legacy.ecology','src/world.c','Grille torique, nourriture locale et repousse; entretien, mouvement, ingestion, reproduction, prédation et extinction.','native-supported'),
 ('legacy.vm','src/vm.c','Deux registres octets, pointeur, soma éditable et budget VM; immédiats et branchements selon ISA originale.','native-supported'),
 ('legacy.mutation','src/mutate.c','Substitution par octet, duplication/indels, croisement et paramètres évolutifs portés par le génome selon profil.','native-supported'),
 ('legacy.labs','src/labs.c','Vingt dispositifs de sélection et métriques, contextes distincts du monde ouvert.','historical-context'),
 ('legacy.labs-human','src/labs_h.c','Pliage HP, médicament, SEIR, biens communs, réparation, évolution et apprentissage: récompenses propres aux bancs.','historical-context'),
 ('terrarium.archipelago','experiments/terrarium.c','Îles, migration en anneau, environnement Markov et émission sociale du voisin au tick précédent.','historical-context'),
 ('terrarium.forces','experiments/terrarium.c','Espèce dérivée des opcodes du germen; masque de rôles ATTACK/YIELD/EMIT/XOR/AND/SUB/NEURAL/ADD.','historical-context'),
 ('terrarium.causes','experiments/terrarium.c','YIELD nourrit; AND/ADD attirent vers semblables; XOR détourne des différents; SUB draine leur énergie.','historical-context'),
 ('terrarium.bricks','experiments/terrarium.c','EMIT pose une brique à recette; carré 2×2 mixte irrigue; seuil de briques ouvre une île.','historical-context'),
 ('terrarium.stress','experiments/terrarium.c','Mutation multipliée par 1+7×stress; stress combine faible énergie, pauvreté et densité.','historical-context'),
 ('terrarium.attention','experiments/terrarium.c','Calcul avant action dépose chaleur; choix dépense chaleur et modifie nourriture, écho et voisins.','historical-context'),
 ('terrarium.traits','experiments/terrarium.c','Traits mesurés depuis instructions exécutées et relations; fréquence5–95%, non-redondance et persistance.','historical-context'),
 ('terrarium.expense','experiments/terrarium.c','Archipel plein: un carré mixte ne nourrit plus et une brique est retirée au relevé.','historical-context'),
 ('terrarium.causal-signals','experiments/terrarium.c','Signal aux siens déplace nourriture; signal aux autres laisse une marque perceptible.','historical-context'),
 ('terrarium.season','experiments/terrarium.c','Saison module repousse et porte de ferme/technologie.','historical-context'),
 ('terrarium.dynamic-laws','experiments/terrarium.c','ABI LoiMonde charge des lois .so neuves au relevé; contrat limité aux champs exposés.','historical-context'),
 ('rencontres.exchange','experiments/rencontres-ligne/echange.c','Deux voisins échangent un segment de soma et germen sans changer la population, coût et repos.','historical-context'),
 ('rencontres.nursery','experiments/rencontres-ligne/naissance.c','Deux parents financent un œuf en saison, incubation et éclosion locale sans supprimer adulte.','historical-context'),
 ('rencontres.meeting','experiments/rencontres-ligne/rendezvous.c','Marques attirantes, test comportemental capteur+STEP isolé, coût du mouvement et expiration.','historical-context'),
 ('rencontres.senescence','experiments/rencontres-ligne/vivre.c','Vieillissement et extinction définis par bras expérimental; aucun réensemencement.','historical-context'),
 ('tronc.osmo','experiments/tronc-ligne/osmo.c','Au relevé transfère une nourriture d’une case riche vers sa voisine est, conserve la somme.','historical-context'),
 ('tronc.competition','experiments/tronc-ligne/competition.c','Au relevé transfère une unité de l’île la plus nourrie vers la plus pauvre.','historical-context'),
 ('tronc.heritage','experiments/tronc-ligne/heritage.c','Réécrit marque1 en (0,0) sur chaque île; ne rejoue pas les tables locales.','historical-context'),
 ('tronc.memoire','experiments/tronc-ligne/memoire.c','Baisse de population dépose chaleur; mémoire du relevé précédent et réarmement à la hausse.','historical-context'),
 ('tronc.renaissance','experiments/tronc-ligne/renaissance.c','Sous1000 individus ajoute huit nourritures; ne recrée pas d’organismes.','historical-context'),
 ('tronc.lecture','experiments/tronc-ligne/lecture.c','Observation une fois du nombre de briques et carrés mixtes, sans retrait.','historical-context'),
 ('tronc.trace','experiments/tronc-ligne/trace.c','Marque2 sur coin de carré2×2 mixte, sauf (0,0) réservé à héritage.','historical-context'),
 ('relief.material','experiments/relief-20261025/relief.c','Conservation exacte minéral+nourriture+détritus+construction+cache+corps+réserve; énergie ouverte.','native-supported'),
 ('relief.climate','experiments/relief-20261025/relief.c','Altitude, roche, pente, bords réfléchissants; lumière saisonnière, pluie, humidité et diffusion.','native-supported'),
 ('relief.life','experiments/relief-20261025/relief.c','Corps/réserve/énergie, abri et sécheresse; division locale, héritage partiel mémoire, mutations de bits et âge maximal.','native-supported'),
 ('relief.witnesses','experiments/relief-20261025/relief.c','Témoins sans perception ou mémoire sur copies; dépendance immédiate seulement, aucune preuve de raisonnement.','native-supported'),
 ('transmission.memory','experiments/transmission-20261026/transmission.c','RAM4096, code modifiable512, pointeur16bits, deux registres, huit artefacts et livres; copie mémoire à reproduction.','native-supported'),
 ('transmission.preservation','experiments/transmission-20261026/transmission.c','Restitution explicite: égalité exacte longueur et octets; bonus énergétique avec coût/plafond/cooldown.','native-supported'),
 ('transmission.mail','experiments/transmission-20261026/transmission.c','Sortie binaire et courrier limité; provenance, identité, checksum; les lettres ne sont jamais exécutées.','native-supported'),
 ('transmission.library','experiments/transmission-20261026/transmission.c','Requête de titre Grokipedia seulement via broker extérieur strict; aucun réseau arbitraire de la VM. Broker dédié à ce monde, pont de fichiers seul.','native-broker-supported'),
 ('quantique.q1','experiments/quantique/monde.cpp','Octets continus classiques, champ rotor de phase, potentiel attractif/répulsif, liens durables et projection de Born après boucle.','historical-context'),
 ('quantique.q2','experiments/quantique2/monde2.cpp','Q1+bain Langevin, queue répulsive SALR, relaxation/renormalisation du champ, empreinte dans octet libre, filiation des assemblages.','historical-context'),
 ('choix.m1-m2-e1','experiments/choix.c','Consultation coûteuse indice fiable75%, choix différé irrévocable; M2 composition; E1 métabolisme et reproduction sans réensemencement.','historical-context'),
 ('raison.ten-environments','experiments/raison.c','Mémoire, composition, inversion, observation, patience, chaînage, transfert, signal, autrui, relation; bras information et témoins.','historical-context'),
 ('universe.tables','experiments/universe_search2.c','Tables aléatoires octet→primitive, coûts et dérive des univers; exécution et réplication sous lois propres.','historical-context'),
 ('universe.keepers','experiments/law_keepers.c','READ_LAW et WRITE_LAW avec jeton limité au débit de dérive, contre témoins écritures jetées/redirigées.','historical-context'),
 ('universe.local-laws','experiments/local_laws.c','Chaque individu porte sa table; réécrire ≥moitié de l’autre transmet sa table de lois, sans oracle de fidélité.','historical-context'),
 ('innovant.cellular','experiments/univers_innovant.c','Huit familles d’automates locaux octets, tirages et test de motifs persistants, sans ISA ni génome.','historical-context'),
]


def _source(path):
    p = ROOT / path
    return {'path': path, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None}


def _lead(path):
    text = (ROOT / path).read_text(errors='replace')
    m = re.search(r'/\*(.*?)\*/', text, re.S)
    lead = re.sub(r'\s+', ' ', m.group(1).replace('*','')).strip() if m else text.splitlines()[0]
    return lead[:1600]


def generate(seed):
    """Deterministic bounded laws + complete table; no imports of executable old worlds.
    native-supported = original adapter provides capacity, not every historical mode active.
    historical-context = source/spec retained but not implemented by this new runtime.
    Only runtime can attest which supported rules are actually invoked.
    """
    rng = random.Random(seed)
    laws=[]
    def add(id, spec, descr, path, status='historical-context', params=None):
        laws.append({'id':id,'spec':spec,'descr':descr,'params':params or {},'source':_source(path),'status':status})
    for id,path,descr,status in FAMILIES:
        if id.startswith(('terrarium.','tronc.')):
            status='native-supported'
        add(id, _lead(path), descr, path, status)
    lab_text=(ROOT/'src/labs.c').read_text().split('const LabDef LABS[N_LABS] =',1)[1]
    for name,title,fitness in re.findall(r'^    \{ *"([^"\n]+)",\s*"([^"\n]+)",\s*"([^"\n]+)"', lab_text,re.M):
        add('lab.'+name,fitness,title+'; fonction de sélection du banc historique, pas une récompense active du monde ouvert.','src/labs.c','historical-context')
    for name,spec in {
        'memoire':'Indice fiable75% au tick0, décision au tick5; témoin indice aléatoire.',
        'composition':'u puis v0..19; cible parité(u XOR v), décision tick6;80couples réservés aux tests.',
        'inversion':'Indice exact, règle cachée basculant tous16cycles; témoin règle indépendante à chaque épisode.',
        'observation':'Signal gratuit annonce fiabilité de l’indice payant; témoin signal sans rapport.',
        'patience':'Signal annonce récompense différée+18deux épisodes plus tard; prendre immédiatement+10.',
        'chainage':'Déverrouillage tick1 nécessaire à indice tick2 et décision tick5; témoin paiement sans déverrouillage.',
        'transfert':'Entrée x0..255, cible bit4; quartets bas12..15 réservés aux tests.',
        'signal':'Émetteur voit cible; EMIT ticks1–2 transmis au récepteur tick3; témoin canal bruité.',
        'autrui':'Observateur voit action d’un acteur jouant la mémoire au tick3; témoin action aléatoire.',
        'relation':'x puis y, cible égalité x=y; valeurs192..255 réservées aux tests.'
    }.items():
        add('raison.'+name,spec,'Milieu informationnel historique avec bras témoin; protocole conservé, pas activé dans l’écologie spatiale.','experiments/raison/PROTOCOLE.md','historical-context')
    for family,names,path in [('legacy',CORE,'src/vm.c'),('relief',RELIEF,'experiments/relief-20261025/relief.c'),('transmission',TRANSMISSION,'experiments/transmission-20261026/transmission.c')]:
        for op,name in enumerate(names):
            add(f'{family}.isa.{op:02x}',f'{family} opcode {op} ({name}). '+EFFECTS[family][op],f'Instruction {name}; sémantique et adressage propres au dialecte {family}, sans conversion de génome.',path,'native-supported',{'opcode':op})
    for op,name in [(240,'RPUSH'),(241,'RPEEK')]:
        add(f'legacy.regs.{op:02x}',f'{name} imm8 sous cfg.regs; intron sinon.', 'Pile de quatre registres de vie; sémantique originale, pas une conversion des génomes.','src/vm.c','native-supported',{'opcode':op})
    # Enumerate every remaining experiment source, preserving exact source hashes and comments.
    # This catches the soup/ensemble/sequence/transplant and bench families beyond the major worlds.
    known={v[1] for v in FAMILIES}
    sources=sorted(set(list((ROOT/'experiments').glob('*.c'))+list((ROOT/'experiments').glob('*/*.c'))+list((ROOT/'experiments').glob('*/*.cpp'))))
    excluded=('generateur-20261027','build','resultats','validation','corrections','relief-comparatif','relief-observatoire')
    for p in sources:
        rel=str(p.relative_to(ROOT))
        if rel in known or any(x in rel for x in excluded): continue
        add('archive.'+rel.removeprefix('experiments/').rsplit('.',1)[0].replace('/','.'),_lead(rel),'Famille historique conservée avec sa source exacte; dispositif original distinct, non invoqué automatiquement.',rel)
    add('controller.heterogeneous-habitats','Trois compartiments legacy/Relief/Transmission. Chaque créature garde son enregistrement natif; aucune traduction des génomes et aucune reproduction entre ISA.', 'La chimie et un pont sensoriel relient ces habitats; ce monde est un système couplé, pas une grille native unique.','experiments/generateur-20261027/generator.py','runtime-supported')
    add('controller.sensory-bridge','Les émissions natives alimentent une chimie commune; son octet dominant devient stimulus historique et signal Relief; chaînes exactes dans artefact partagé7 de Transmission à la frontière de lot.', 'Pont extérieur conçu: il peut écraser le signal Relief et l’artefact7 au début de lot. Lire ce pont ne prouve pas comprendre les autres ISA.','experiments/generateur-20261027/generator.py','runtime-supported')
    add('controller.live-updates','Patches JSON validés, empreinte SHA256 et reçu; paramètres numériques natifs et règles déclaratives left/right/product/rate/spec/descr appliqués à la frontière de lot.', 'Les instructions injectables sont des règles de réaction de la chimie. Elles n’ajoutent pas automatiquement un nouvel opcode C aux ISA natives.','experiments/generateur-20261027/generator.py','runtime-supported')
    add('controller.selection-injection','Initialisation exige420 enregistrements avec SHA256 vérifié; injection native permet ensuite d’ajouter des enregistrements compatibles dans une place libre.', 'Identité/provenance conservées dans manifeste; identifiants, parent de fondation et position peuvent être réassignés par adaptateur. Aucun ancien monde n’est repris.','experiments/generateur-20261027/generator.py','runtime-supported')
    add('controller.persistence','Verrou écrivain, vérification SHA256 ELF, fichiers atomiques fsync+rename. Avant lot: sauvegarde des3checkpoints, chimie, statut et stimuli puis marque READY. Reprise restaure ces fichiers si READY subsiste; fin de lot retire durablement READY.', 'Retour arrière coordonné des états principaux; effets externes du broker/courrier et journaux doivent être réconciliés séparément. Extinction totale termine la boucle sans réensemencement.','experiments/generateur-20261027/generator.py','runtime-supported')
    # Adapter knobs are effective native fields, not random labels on inert context.
    native_params = {
        'relief': {'season': rng.randint(1024,8192)},
        'legacy': {'food_regrow':rng.randint(2,8),'split_energy':rng.randint(100,180),'mut_rate':rng.uniform(0.002,0.02),'dup_rate':rng.uniform(0.005,0.04),'max_pop':rng.randint(768,1536),'ops_per_tick':rng.randint(12,24),'regs':rng.randint(0,1),'germline':rng.randint(0,1),'attack_steal':rng.randint(4,16),'forces':3,'stress':1,'attention':1,'osmo':rng.randint(1,3),'season_amp':rng.uniform(0.1,0.8),'season_period':rng.randint(1024,8192),'migrate':rng.randint(256,2048),'depense':1,'causales':1,'saison':1,'tronc':1},
        'transmission': {'native_eat_gain':rng.randint(5,9),'native_mutation_divisor':rng.randint(4,16),'native_max_age':rng.randint(16384,32768)},
    }
    for dialect, params in native_params.items():
        path = {'relief':'experiments/relief-20261025/relief.c','legacy':'src/world.c','transmission':'experiments/transmission-20261026/transmission.c'}[dialect]
        add('native.parameters.'+dialect, 'Valeurs tirées dans des bornes validées par adaptateur; params.tsv appliqué au tick de frontière.', 'Paramètres numériques mutables du dialecte; tout changement est enregistré sans traduire les génomes. Les drapeaux de familles héritées restent activés à la genèse; leurs variables numériques sont tirées.',path,'native-supported',params)
    # Explicit numerical ranges, drawn only at genesis; updates must be journaled by caller.
    chemistry={'reaction_cost':rng.randint(1,4),'bond_threshold':rng.randint(2,6),'catalyst_gain':rng.randint(1,4),'decay_period':rng.randint(128,1024),'reaction_period':rng.randint(2,8),'max_chain':rng.randint(8,32),'instruction_budget':rng.randint(8,24)}
    add('chemistry.bytes','Les256 valeurs ont masse, charge, valence, affinité, catalyse, rétention et dureté; table déterministe par graine.', 'Ces propriétés sont une physique conçue, distincte des significations natives des opcodes; aucune valeur est dépourvue de propriétés.', 'experiments/generateur-20261027/laws.py','runtime-supported',chemistry)
    add('chemistry.composition','Par lot,256//reaction_period collisions tirées. Charge: abs(charge[a]+charge[b])≤bond_threshold; valences positives. Probabilité de liaison=bond_probability×(0,5+(affinity[a] XOR affinity[b])/510). Deux atomes deviennent chaîne ordonnée conservant leurs valeurs et leurs masses; doublons de paires refusés.', 'Catalyse=min(catalysis), force=min(valence)+min(hardness), rétention=min(retention). La croissance ajoute un atome libre avec probabilité0,02, jusqu’à max_chain; propriétés initiales de la paire restent fixes. Aucune émission n’est exécutée comme programme hôte.', 'experiments/generateur-20261027/generator.py','runtime-supported',chemistry.copy())
    add('chemistry.reaction-rules','Règle déclarative left+right→product avec rate; au plus min(1+min(catalyst_gain,nombre_catalyseurs),instruction_budget) essais par règle et lot. Différence de masse au réservoir; produit plus lourd exige réservoir suffisant.', 'Énergie chimique extérieure: ajoute instruction_budget par lot, plafond4096. Réaction coûte reaction_cost et exige énergie disponible. Les artefacts à catalyse>0 augmentent les essais; un artefact tiré reçoit compteur d’usages. Fonction conçue; pas de savoir ni technologie démontrés.', 'experiments/generateur-20261027/generator.py','runtime-supported')
    add('chemistry.decay','Probabilité de décomposition par lot=1/(decay_period×max(1,retention+strength/4)); tous les octets de la chaîne retournent libres.', 'Masse totale vérifiée à chaque lot: atomes libres+chaînes+réservoir =8×somme des256masses + masse acceptée depuis émissions natives. Entrées externes mesurées, aucune conservation commune aux trois physiques natives.', 'experiments/generateur-20261027/generator.py','runtime-supported')
    properties=[]
    for value in range(256):
        properties.append({'value':value,'mass':rng.randint(1,16),'charge':rng.randint(-4,4),'valence':rng.randint(1,4),'affinity':rng.randrange(256),'catalysis':rng.randint(0,4),'retention':rng.randint(1,32),'hardness':rng.randint(1,16)})
    return {'schema':'octet-laws.v1','seed':seed,'laws':laws,'byte_properties':properties,'chemistry':chemistry,'native_params':native_params,'source_inventory_complete_scope':'C/C++ experiment sources at root and depth1 plus native core and explicit world families; experimental wrappers/tests excluded; not a claim that all source semantics are executable.'}


def reaction(a,b,properties,params):
    """Pure candidate reaction. Caller owns inventory conservation and energy payment."""
    pa,pb=properties[a],properties[b]
    if abs(pa['charge']+pb['charge']) > params['bond_threshold']:
        return None
    return {'product':(a+b+pa['affinity']+pb['affinity'])%256,'cost':params['reaction_cost'],'strength':min(pa['valence'],pb['valence'])+min(pa['catalysis'],pb['catalysis'])}
