#!/usr/bin/env python3
"""Export lossless closed Relief events to Releases; read back before eviction.
Only events-*.tsv.{gz,xz} older than one hour; retain the two newest gzip
segments for the observatory. No checkpoint, live journal or series is touched.
"""
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path
import tarfile
import time
import urllib.request
import urllib.error

HIST = Path(os.environ.get('HISTOIRE', '/histoire'))
REPO = 'danielfebrero/octet-adn'
TOKEN = Path('/run/secrets/github_pat').read_text().strip()
API = 'https://api.github.com/repos/' + REPO
LIMIT = 480 * 1024**2

def api(path, data=None, method=None):
    payload = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(API + path, data=payload, method=method,
        headers={'Authorization': 'Bearer ' + TOKEN, 'Accept': 'application/vnd.github+json',
                 'Content-Type': 'application/json', 'User-Agent': 'relief-history-archive'})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.load(r)

def sha(stream):
    h = hashlib.sha256()
    while True:
        b = stream.read(1024**2)
        if not b: return h.hexdigest()
        h.update(b)

def local_sha(path):
    with path.open('rb') as f: return sha(f)

def remote_sha(url):
    with urllib.request.urlopen(url, timeout=120) as r: return sha(r)

def upload(release, path):
    # No overwrite: existing assets must match the local copy exactly.
    assets = api('/releases/%s/assets?per_page=100' % release['id'])
    old = next((a for a in assets if a['name'] == path.name), None)
    if old is not None: return old
    url = release['upload_url'].split('{')[0] + '?name=' + path.name
    with path.open('rb') as f:
        req = urllib.request.Request(url, data=f, method='POST', headers={
            'Authorization': 'Bearer ' + TOKEN, 'Content-Type': 'application/octet-stream',
            'Content-Length': str(path.stat().st_size), 'User-Agent': 'relief-history-archive'})
        with urllib.request.urlopen(req, timeout=600) as r: return json.load(r)

def log(record):
    record['date'] = dt.datetime.now(dt.timezone.utc).isoformat()
    line = json.dumps(record, sort_keys=True)
    with (HIST / 'releases.jsonl').open('a') as f:
        f.write(line + '\n'); f.flush(); os.fsync(f.fileno())
    print(line, flush=True)

def sync_dir():
    fd = os.open(HIST, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)

def archive(group):
    first = group[0].name.split('.')[0]
    last = group[-1].name.split('.')[0]
    name = 'relief-' + first + '--' + last
    tar = HIST / (name + '.tar')
    manifest = HIST / (name + '.json')
    entries = [{'name': p.name, 'size': p.stat().st_size, 'sha256': local_sha(p)} for p in group]
    with tarfile.open(tar, 'w') as out:
        for p in group: out.add(p, arcname=p.name, recursive=False)
    # Verify each member against its original digest, before upload.
    with tarfile.open(tar) as src:
        for entry in entries:
            assert sha(src.extractfile(entry['name'])) == entry['sha256']
    digest = local_sha(tar)
    manifest.write_text(json.dumps({'archive': tar.name, 'sha256': digest,
        'files': entries, 'format': 'original compressed segments in POSIX tar'}, indent=2) + '\n')
    tag = 'relief-history-' + first.removeprefix('events-')[:8]
    try: release = api('/releases/tags/' + tag)
    except urllib.error.HTTPError as exc:
        if exc.code != 404: raise
        release = api('/releases', {'tag_name': tag, 'target_commitish': 'main',
            'name': 'Relief — histoire ' + tag[-8:], 'make_latest': 'false',
            'body': 'Segments fermés originaux, sans perte. Chaque TAR a un manifeste JSON avec SHA-256 de l’archive et de chaque segment. Le VPS relit intégralement les assets avant retrait local. État vivant et ELF conservés sur le VPS.'})
    asset = upload(release, tar)
    meta = upload(release, manifest)
    assert asset['size'] == tar.stat().st_size
    assert remote_sha(asset['browser_download_url']) == digest, 'remote TAR mismatch; originals retained'
    assert remote_sha(meta['browser_download_url']) == local_sha(manifest), 'remote manifest mismatch'
    # Recheck ALL originals before removing ANY; fail closed on concurrent changes.
    for p, entry in zip(group, entries):
        assert p.stat().st_size == entry['size'] and local_sha(p) == entry['sha256']
    log({'status': 'verified', 'url': asset['browser_download_url'], 'manifest_url': meta['browser_download_url'],
         'sha256': digest, 'files': entries, 'bytes': sum(e['size'] for e in entries)})
    for p in group: p.unlink()
    sync_dir()
    tar.unlink(); manifest.unlink(); sync_dir()
    log({'status': 'evicted', 'archive': name, 'segments': len(entries), 'bytes': sum(e['size'] for e in entries)})

def cycle():
    with (HIST / '.archive.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        gz = sorted(HIST.glob('events-*.tsv.gz'))
        keep = set(gz[-2:])
        files = sorted(p for p in list(HIST.glob('events-*.tsv.gz')) + list(HIST.glob('events-*.tsv.xz'))
                       if p not in keep and time.time() - p.stat().st_mtime > 3600)
        group = []; size = 0
        for p in files:
            if group and size + p.stat().st_size > LIMIT:
                archive(group); group = []; size = 0
            group.append(p); size += p.stat().st_size
        if group: archive(group)

if __name__ == '__main__':
    import sys
    while True:
        try: cycle()
        except Exception as exc:
            log({'status': 'ERREUR', 'error': str(exc)})
            if '--once' in sys.argv: raise
        if '--once' in sys.argv: break
        time.sleep(3600)
