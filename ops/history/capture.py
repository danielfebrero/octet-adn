#!/usr/bin/env python3
"""Preserve report-ring versions of existing worlds without restarting them."""
import hashlib, json, os, time
from pathlib import Path
ROOT = Path(os.getenv('OCTET_ROOT', '/repo'))
STATE = Path(os.getenv('OCTET_ARCHIVE_STATE', '/state'))
def capture():
    n = 0
    for reports in (ROOT / 'experiments').glob('vie-*/reports'):
        for p in reports.glob('*.json'):
            try:
                data = p.read_bytes(); value = json.loads(data)
                batch = int(value['batches'])
                dest = STATE / 'captured' / reports.parent.name / ('%016d-%s.json' % (batch, hashlib.sha256(data).hexdigest()))
                # A durable marker avoids recreating already-offloaded snapshots.
                marker = STATE / 'capture-seen' / reports.parent.name / p.name
                digest = hashlib.sha256(data).hexdigest()
                if marker.exists() and marker.read_text() == digest: continue
                dest.parent.mkdir(parents=True, exist_ok=True)
                temp = dest.with_suffix('.tmp'); temp.write_bytes(data)
                with temp.open('rb') as f: os.fsync(f.fileno())
                temp.replace(dest)
                marker.parent.mkdir(parents=True, exist_ok=True)
                marker.write_text(digest); n += 1
            except (FileNotFoundError, ValueError, KeyError): continue
    return n
if __name__ == '__main__':
    while True:
        try:
            n = capture()
            if n: print(json.dumps({'captured': n, 'at': time.time()}), flush=True)
        except Exception as e: print(json.dumps({'ERROR': str(e)}), flush=True)
        time.sleep(30)
