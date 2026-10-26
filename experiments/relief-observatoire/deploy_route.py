"""Persist the mini-app routes, then load that exact Caddyfile.

Run in Caddy's network namespace, host deploy mounted at /deploy.
Refuse runtime/file drift rather than discarding somebody else's routes.
"""
import json, urllib.request, sys
from pathlib import Path
sys.path.insert(0, '/deploy/miniapps')
from install_routes import patch

base = 'http://127.0.0.1:2019'
def post(path, body, kind):
    req = urllib.request.Request(base+path, data=body, headers={'Content-Type':kind}, method='POST')
    with urllib.request.urlopen(req) as r: return r.read()
def adapt(text):
    config = json.loads(post('/adapt', text.encode(), 'text/caddyfile'))['result']
    # The API adapter has no filename; CLI reload adds the absolute config path
    # to file_server.hide. Match that metadata, without ignoring route changes.
    def filename(value):
        if isinstance(value, dict):
            if value.get('handler') == 'file_server' and 'hide' in value:
                value['hide'] = ['/etc/caddy/Caddyfile' if x == './Caddyfile' else x for x in value['hide']]
            for child in value.values(): filename(child)
        elif isinstance(value, list):
            for child in value: filename(child)
    filename(config)
    return config

p = Path('/deploy/Caddyfile')
old = p.read_text()
with urllib.request.urlopen(base+'/config/') as r: live = json.load(r)
if live != adapt(old): raise RuntimeError('Caddy runtime differs from persisted file; reconcile before loading')
new = patch(old)
config = adapt(new)
Path('/deploy/Caddyfile.before-relief').write_text(old)
p.write_text(new)
try:
    post('/load', json.dumps(config).encode(), 'application/json')
except Exception:
    p.write_text(old)
    raise
print('Routes Relief et RPC persistées et chargées depuis le même Caddyfile.')
