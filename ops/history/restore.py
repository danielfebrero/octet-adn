#!/usr/bin/env python3
"""Restore one archived path from all daily Releases, into a NEW output file."""
import argparse, hashlib, io, json, os, tarfile, urllib.request
from pathlib import Path
API = 'https://api.github.com/repos/danielfebrero/octet-adn'
def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent':'octet-restore'}), timeout=600) as f: return f.read()
def records(path):
    page = 1
    while True:
        releases = json.loads(get(API + '/releases?per_page=100&page=%d' % page))
        for release in releases:
            if not release['tag_name'].startswith('octet-history-'): continue
            asset_page = 1; assets = []
            while True:
                rows = json.loads(get(release['assets_url'] + '?per_page=100&page=%d' % asset_page)); assets += rows
                if len(rows) < 100: break
                asset_page += 1
            by_name = {a['name']:a for a in assets}
            for asset in assets:
                if not (asset['name'].startswith('blocks-') and asset['name'].endswith('.json')): continue
                meta = json.loads(get(asset['browser_download_url']))
                for entry in meta['files']:
                    if entry['path'] == path: yield meta, entry, by_name[meta['archive']]['browser_download_url']
        if len(releases) < 100: break
        page += 1

def restore(rows, output):
    rows = list(rows)
    if not rows: raise ValueError('Path absent from Releases')
    # A moving file can have partial observed versions: select latest COMPLETE snapshot.
    versions = {}
    for meta, entry, url in rows:
        versions.setdefault(tuple(entry['signature']), {})[entry['offset']] = (meta, entry, url)
    chosen = None
    for sig in sorted(versions, key=lambda s:(s[3],s[4]), reverse=True):
        parts = sorted(versions[sig].values(), key=lambda row:row[1]['offset'])
        offset = 0
        for _, entry, _ in parts:
            if entry['offset'] != offset: break
            offset += entry['size']
        else:
            if offset == sig[2]: chosen = parts; break
    if chosen is None: raise ValueError('No complete version; restore refused')
    output = Path(output)
    if output.exists(): raise FileExistsError(output)
    temporary = output.with_name(output.name + '.restoring')
    # Exclusive creation, no overwrite of checkpoints or existing output.
    try:
        with temporary.open('xb') as dest:
            cached_url = None
            for meta, entry, url in chosen:
                if url != cached_url:
                    data = get(url)
                    if hashlib.sha256(data).hexdigest() != meta['sha256']: raise ValueError('Archive checksum mismatch')
                    archive = tarfile.open(fileobj=io.BytesIO(data), mode='r:gz'); cached_url = url
                body = archive.extractfile(entry['member']).read()
                if len(body) != entry['size'] or hashlib.sha256(body).hexdigest() != entry['sha256']: raise ValueError('Block checksum mismatch')
                dest.write(body)
            dest.flush(); os.fsync(dest.fileno())
        os.chmod(temporary, chosen[0][1]['mode'])
        os.link(temporary, output); temporary.unlink()
    except BaseException:
        if temporary.exists(): temporary.unlink()
        raise
    return output.stat().st_size
if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('path'); p.add_argument('output', type=Path)
    a = p.parse_args(); print(restore(records(a.path), a.output), 'bytes restored and verified')
