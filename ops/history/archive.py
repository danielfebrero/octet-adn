#!/usr/bin/env python3
"""Verified, lossless offload. No live checkpoint or open log is ever evicted."""
import datetime, fcntl, hashlib, io, json, os, sqlite3, tarfile, time
from pathlib import Path
import urllib.request, urllib.error
ROOT = Path(os.getenv('OCTET_ROOT', '/repo'))
BACKUPS = Path(os.getenv('OCTET_BACKUPS', '/backups'))
STATE = Path(os.getenv('OCTET_ARCHIVE_STATE', '/state'))
API = 'https://api.github.com/repos/danielfebrero/octet-adn'
BLOCK = 16 * 1024**2
BATCH = 128 * 1024**2
TOKEN = ''
def sha(data): return hashlib.sha256(data).hexdigest()
def signature(p):
    s = p.stat()
    return [s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns]
def request(url, data=None, binary=False):
    headers = {'User-Agent': 'octet-history', 'Accept': 'application/vnd.github+json'}
    if url.startswith(('https://api.github.com/', 'https://uploads.github.com/')):
        headers['Authorization'] = 'Bearer ' + TOKEN
    if data is not None:
        if not binary: data = json.dumps(data).encode()
        headers.update({'Content-Type': 'application/octet-stream' if binary else 'application/json', 'Content-Length': str(len(data))})
    return urllib.request.urlopen(urllib.request.Request(url, data=data, headers=headers), timeout=600)
def api(path, data=None):
    with request(API + path, data) as f: return json.load(f)
def remote_sha(url):
    h = hashlib.sha256()
    with request(url) as f:
        while b := f.read(1024**2): h.update(b)
    return h.hexdigest()
def log(status, **fields):
    row = dict(at=datetime.datetime.now(datetime.timezone.utc).isoformat(), status=status, **fields)
    with (STATE / 'releases.jsonl').open('a') as f:
        f.write(json.dumps(row) + '\n'); f.flush(); os.fsync(f.fileno())
    print(json.dumps(row), flush=True)
def get_release():
    tag = 'octet-history-' + time.strftime('%Y%m%d', time.gmtime())
    try: return api('/releases/tags/' + tag)
    except urllib.error.HTTPError as e:
        if e.code != 404: raise
    return api('/releases', dict(tag_name=tag, target_commitish='main', make_latest='false',
        name='Octet-ADN — archives ' + tag[-8:], body='Archives intégrales, blocs et manifestes SHA-256 avec chemins originaux. Téléchargement intégral de contrôle avant suppression locale. Restauration : ops/history/restore.py. Snapshots par fichier, sans promesse de cohérence atomique entre fichiers vivants.'))
class Export:
    def __init__(self, db):
        self.db = db; self.rel = None; self.assets = {}; self.reset()
    def reset(self):
        self.entries = []; self.size = 0; self.tar = None; self.path = STATE / 'pending.tar.gz'
    def add(self, key, sig, mode, offset, data):
        if self.tar is None: self.tar = tarfile.open(self.path, 'w:gz', compresslevel=1)
        member = str(len(self.entries)); info = tarfile.TarInfo(member); info.size = len(data)
        self.tar.addfile(info, io.BytesIO(data))
        self.entries.append(dict(path=key, signature=sig, mode=mode, offset=offset, size=len(data), sha256=sha(data), member=member))
        self.size += len(data)
        if self.size >= BATCH or len(self.entries) >= 2000: self.flush()
    def flush(self):
        if self.tar is None: return
        self.tar.close(); payload = self.path.read_bytes(); digest = sha(payload)
        name = 'blocks-' + digest + '.tar.gz'
        manifest = json.dumps(dict(format='octet-blocks-v1', archive=name, sha256=digest, files=self.entries), sort_keys=True).encode()
        if self.rel is None:
            self.rel = get_release(); page = 1
            while True:
                assets = api('/releases/%s/assets?per_page=100&page=%d' % (self.rel['id'], page))
                self.assets.update({a['name']: a for a in assets})
                if len(assets) < 100: break
                page += 1
        urls = []
        for asset_name, data in ((name, payload), ('blocks-' + digest + '-' + sha(manifest) + '.json', manifest)):
            asset = self.assets.get(asset_name)
            if asset is None:
                url = self.rel['upload_url'].split('{')[0] + '?name=' + asset_name
                with request(url, data, True) as f: asset = json.load(f)
                self.assets[asset_name] = asset
            if asset['size'] != len(data) or remote_sha(asset['browser_download_url']) != sha(data):
                raise RuntimeError('Remote SHA-256 mismatch: ' + asset_name)
            urls.append(asset['browser_download_url'])
        log('verified', archive=urls[0], manifest=urls[1], bytes=self.size, blocks=len(self.entries))
        for e in self.entries:
            self.db.execute('INSERT OR REPLACE INTO blocks VALUES (?,?,?,?,?)', (e['path'], json.dumps(e['signature']), e['offset'], e['size'], e['sha256']))
        self.db.commit(); self.path.unlink(); self.reset()
def cold_worlds():
    # Only the explicitly retired worlds of 25 October. Never infer extinction by age.
    return set(json.loads((Path(__file__).parent / 'cold-worlds.json').read_text()))
def evictable(key, age, cold):
    if key.startswith(('captured/', 'ledger/')): return True
    if key.startswith('backups/'): return key.startswith('backups/octet-adn-20261025-20f62d86/')
    rel = key.removeprefix('repo/')
    if any(rel.startswith(w + '/') for w in cold): return True
    parts = Path(rel).parts
    if 'histoire' in parts and 'reports' in parts and rel.endswith('.json'): return True
    if 'vie-relief-20261025' in parts:
        return 'histoire' in parts and Path(rel).name.startswith('series-') and rel.endswith(('.tsv.gz', '.tsv.xz')) and age > 86400
    if 'histoire' in parts and Path(rel).name.startswith('events-') and rel.endswith(('.tsv.gz', '.tsv.xz')): return age > 86400
    if 'mail' in parts and age > 7 * 86400:
        try:
            mail = ROOT / Path(rel).parent
            cursor = int((mail.parent / 'pickups/cursor').read_text())
            return int(Path(rel).stem) <= cursor and Path(rel).suffix in ('.bin', '.tsv')
        except (FileNotFoundError, ValueError): return False
    return 'snapshots' in parts and age > 7 * 86400

def sources():
    ledger = STATE / 'ledger'
    if ledger.exists(): yield ledger, 'ledger'
    captured = STATE / 'captured'
    if captured.exists(): yield captured, 'captured'
    for base, prefix in ((ROOT / 'experiments', 'repo/experiments'), (ROOT / 'labs_data', 'repo/labs_data')):
        if base.exists(): yield base, prefix
    for base in sorted(BACKUPS.glob('octet-adn-*')):
        if base.is_dir(): yield base, 'backups/' + base.name

def finalize(db, pending, cold):
    removed = 0
    for p, key, sig in pending:
        try:
            if signature(p) != sig:
                log('changed_retry', path=key); continue
            encoded = json.dumps(sig)
            blocks = db.execute('SELECT offset,size,sha FROM blocks WHERE path=? AND signature=? ORDER BY offset', (key, encoded)).fetchall()
            if not blocks or sum(b[1] for b in blocks) != sig[2]: raise RuntimeError('Incomplete remote coverage: ' + key)
            db.execute('INSERT OR REPLACE INTO files VALUES (?,?)', (key, encoded))
            db.execute('DELETE FROM blocks WHERE path=? AND signature!=?', (key, encoded)); db.commit()
            if not evictable(key, time.time() - p.stat().st_mtime, cold): continue
            with p.open('rb') as f:
                for offset, size, digest in blocks:
                    if f.tell() != offset or sha(f.read(size)) != digest: raise RuntimeError('Local SHA mismatch: ' + key)
            if signature(p) != sig: continue
            p.unlink(); removed += sig[2]
            log('evicted', path=key, bytes=sig[2])
            db.execute('DELETE FROM files WHERE path=?', (key,))
            db.execute('DELETE FROM blocks WHERE path=?', (key,)); db.commit()
        except FileNotFoundError: pass
    return removed

def cycle():
    journal = STATE / 'releases.jsonl'
    if journal.exists() and journal.stat().st_size > 8 * 1024**2:
        ledger = STATE / 'ledger'; ledger.mkdir(exist_ok=True)
        journal.rename(ledger / ('releases-%d.jsonl' % time.time_ns()))
    db = sqlite3.connect(STATE / 'index.sqlite')
    db.execute('CREATE TABLE IF NOT EXISTS files (path TEXT PRIMARY KEY, signature TEXT)')
    db.execute('CREATE TABLE IF NOT EXISTS blocks (path TEXT, signature TEXT, offset INTEGER, size INTEGER, sha TEXT, PRIMARY KEY(path,signature,offset))')
    export = Export(db); cold = cold_worlds(); pending = []; count = removed = 0
    for base, prefix in sources():
        for directory, dirs, files in os.walk(base, followlinks=False):
            dirs[:] = sorted(d for d in dirs if d not in ('.git', '__pycache__') and not Path(directory, d).is_symlink())
            for name in sorted(files):
                p = Path(directory, name)
                if 'vie-relief-20261025/histoire' in str(p) and (name.startswith(('events-', 'relief-events-')) or name.endswith('.tar')): continue  # existing locked exporter
                if p.is_symlink() or not p.is_file() or name.endswith(('.lock', '.tmp', '.pid', '.pyc')): continue
                key = prefix + '/' + str(p.relative_to(base))
                try:
                    sig = signature(p); encoded = json.dumps(sig)
                    old = db.execute('SELECT signature FROM files WHERE path=?', (key,)).fetchone()
                    if not old or old[0] != encoded:
                        offset = 0
                        with p.open('rb') as f:
                            while offset < sig[2] or (sig[2] == 0 and offset == 0):
                                data = f.read(min(BLOCK, sig[2] - offset))
                                if not data and sig[2]: raise RuntimeError('Short read: ' + key)
                                have = db.execute('SELECT size,sha FROM blocks WHERE path=? AND signature=? AND offset=?', (key, encoded, offset)).fetchone()
                                if have != (len(data), sha(data)): export.add(key, sig, p.stat().st_mode & 0o777, offset, data)
                                offset += len(data)
                                if not data: break
                        count += 1
                    pending.append((p, key, sig))
                    if len(pending) >= 500:
                        export.flush(); removed += finalize(db, pending, cold); pending = []
                except FileNotFoundError: pass
    export.flush(); removed += finalize(db, pending, cold)
    log('cycle_ok', files=count, evicted_bytes=removed, free_bytes=os.statvfs(STATE).f_bavail * os.statvfs(STATE).f_frsize)
    db.close()
if __name__ == '__main__':
    import sys
    STATE.mkdir(parents=True, exist_ok=True)
    TOKEN = Path('/run/secrets/github_pat').read_text().strip()
    with (STATE / 'archive.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        while True:
            try: cycle()
            except Exception as e:
                log('ERROR', error=str(e))
                if '--once' in sys.argv: raise
            if '--once' in sys.argv: break
            time.sleep(3600)
