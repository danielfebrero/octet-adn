"""Stockage de Relief : recompression sans perte des segments d'histoire fermés, gzip -> xz.

Règles (voir STRATEGIE.md) :
- ne touche que histoire/*.tsv.gz fermés depuis plus de AGE heures (le plus récent reste en gzip : l'observatoire le lit) ;
- écrit d'abord NOM.tsv.xz.tmp, le relit, compare l'empreinte SHA-256 du contenu décompressé à celle du .gz ;
- seulement si elles sont égales : renomme en NOM.tsv.xz, synchronise le dossier, puis retire le .gz ;
- n'efface jamais rien d'autre ; aucune donnée n'est perdue ni résumée ;
- écrit l'état du disque dans histoire/STOCKAGE.txt et une ligne par segment dans histoire/stockage.log.
Le monde vivant, son état et ses journaux ouverts (events.tsv, series.tsv) ne sont pas montés ici.
"""
import gzip, hashlib, lzma, os, sys, time, fcntl, json
from pathlib import Path

HIST = Path(os.environ.get('HISTOIRE', '/histoire'))
AGE = float(os.environ.get('AGE_HEURES', '24')) * 3600
PAR_CYCLE = int(os.environ.get('PAR_CYCLE', '6'))
PAUSE = int(os.environ.get('PAUSE_S', '600'))
ALERTE_GO = float(os.environ.get('ALERTE_GO', '50'))
BLOC = 1 << 20

def sync_dir(p):
    fd = os.open(p, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)

def log(line):
    with open(HIST / 'stockage.log', 'a') as f:
        f.write(time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + '\t' + line + '\n')

def digest(opener, path):
    h = hashlib.sha256(); n = 0
    with opener(path, 'rb') as f:
        while True:
            b = f.read(BLOC)
            if not b: break
            h.update(b); n += len(b)
    return h.hexdigest(), n

def convertir(gz):
    xz = gz.with_suffix('.xz'); tmp = gz.with_name(gz.name[:-3] + '.xz.tmp')
    if xz.exists(): raise RuntimeError(f'{xz.name} existe déjà')
    h = hashlib.sha256(); n = 0
    with gzip.open(gz, 'rb') as src, lzma.open(tmp, 'wb', format=lzma.FORMAT_XZ, preset=9) as dst:
        while True:
            b = src.read(BLOC)
            if not b: break
            h.update(b); n += len(b); dst.write(b)
    with open(tmp, 'rb') as f: os.fsync(f.fileno())
    relu, m = digest(lzma.open, tmp)
    if relu != h.hexdigest() or m != n:
        tmp.unlink(); raise RuntimeError(f'{gz.name} : relecture xz différente, gz conservé')
    st = gz.stat(); os.utime(tmp, (st.st_atime, st.st_mtime))      # garde la date de fermeture du segment
    os.rename(tmp, xz); sync_dir(HIST)
    taille_gz = st.st_size
    gz.unlink(); sync_dir(HIST)
    log(f'{gz.name}\tgz={taille_gz}\txz={xz.stat().st_size}\tbrut={n}\tsha256={relu}')
    return taille_gz, xz.stat().st_size

def etat():
    st = os.statvfs(HIST); libre = st.f_bavail * st.f_frsize / 1e9
    gz = list(HIST.glob('*.tsv.gz')); xz = list(HIST.glob('*.tsv.xz'))
    total = sum(p.stat().st_size for p in gz + xz) / 1e9
    def ferme(p):                                                  # heure de fermeture inscrite dans le nom
        try: return time.mktime(time.strptime(p.name.split('-', 1)[1][:15], '%Y%m%dT%H%M%S')) - time.timezone
        except (IndexError, ValueError): return p.stat().st_mtime
    jour_gz = sum(p.stat().st_size for p in gz if time.time() - ferme(p) < 86400) / 1e9
    exportes = {}
    dernier_export = {}
    try:
        for line in (HIST / 'releases.jsonl').read_text().splitlines():
            row = json.loads(line)
            dernier_export = row
            if row.get('status') == 'verified':
                for entry in row['files']: exportes[entry['name']] = entry['size']
    except FileNotFoundError: pass
    # Keep the production estimate meaningful after offloading; no double counting.
    locaux = {p.name for p in gz + xz}
    for name, size in exportes.items():
        if name not in locaux:
            try: stamp = time.mktime(time.strptime(name.split('-', 1)[1][:15], '%Y%m%dT%H%M%S')) - time.timezone
            except (ValueError, IndexError): continue
            if time.time() - stamp < 86400: jour_gz += size / 1e9
    ratio = 0.45
    try:
        r = [int(c.split('=')[1]) for l in open(HIST / 'stockage.log') for c in l.split('\t') if c.startswith(('gz=', 'xz='))]
        if len(r) >= 2: ratio = sum(r[1::2]) / max(1, sum(r[0::2]))
    except FileNotFoundError: pass
    jour = jour_gz * ratio
    lignes = [f'date={time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}',
              f'disque_libre_go={libre:.1f}', f'histoire_go={total:.3f}', f'segments_gz={len(gz)}', f'segments_xz={len(xz)}',
              f'export_verifie_go={sum(exportes.values()) / 1e9:.3f}', f'segments_exportes={len(exportes)}', f'ecrit_24h_gz_go={jour_gz:.3f}', f'ratio_xz_sur_gz={ratio:.3f}', f'rythme_stocke_go_jour={jour:.3f}', f'jours_restants={libre / max(jour, 1e-3):.0f}', 'projection=sans_exports_futurs_disque_partage',
              'dernier_export=' + dernier_export.get('date', 'aucun'),
              'statut=' + ('ALERTE export GitHub en erreur' if dernier_export.get('status') == 'ERREUR' else ('ALERTE disque < %.0f Go' % ALERTE_GO if libre < ALERTE_GO else 'ok'))]
    tmp = HIST / 'STOCKAGE.txt.tmp'
    tmp.write_text('\n'.join(lignes) + '\n'); os.replace(tmp, HIST / 'STOCKAGE.txt')

def cycle_locked():
    for t in HIST.glob('*.xz.tmp'): t.unlink()                     # reste d'un arrêt brutal : jamais un fichier validé
    vieux = sorted(p for p in HIST.glob('*.tsv.gz') if time.time() - p.stat().st_mtime > AGE)
    faits = 0
    for gz in vieux[:PAR_CYCLE]:
        try: convertir(gz); faits += 1
        except Exception as e: log(f'ERREUR\t{e}'); break
    etat(); return faits

def cycle():
    with (HIST / '.archive.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return cycle_locked()

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'une-fois': print(cycle(), 'segment(s) recompressé(s)'); sys.exit(0)
    while True:
        try: cycle()
        except Exception as e: log(f'ERREUR\t{e}')
        time.sleep(PAUSE)
