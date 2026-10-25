"""Add only /relief to live Caddy routes, preserve all other live handlers.
Run in Caddy's network namespace, /opt/.../deploy mounted at /deploy.
The Caddyfile gets the same additive route for future standard restarts.
"""
import json, urllib.request
from pathlib import Path
base='http://127.0.0.1:2019'
def get(path):return json.load(urllib.request.urlopen(base+path))
config=get('/config/')
Path('/deploy/relief-caddy-before.json').write_text(json.dumps(config))
route={'match':[{'host':['mcp.cdxxotus.com'],'path':['/relief/*']}],
       'handle':[{'handler':'reverse_proxy','upstreams':[{'dial':'relief-observatoire:8774'}]}],'terminal':True}
for name,server in config['apps']['http']['servers'].items():
    if not any(r.get('match')==route['match'] for r in server['routes']):
        server['routes'].insert(0,route)
req=urllib.request.Request(base+'/load',data=json.dumps(config).encode(),headers={'Content-Type':'application/json'},method='POST')
urllib.request.urlopen(req).read()
p=Path('/deploy/Caddyfile');text=p.read_text()
if 'handle /relief/*' not in text:
    Path('/deploy/Caddyfile.before-relief').write_text(text)
    anchor='http://{$MCP_DOMAIN}, {$MCP_DOMAIN} {'
    if anchor not in text:raise RuntimeError('MCP vhost missing')
    text=text.replace(anchor,anchor+'\n\thandle /relief/* {\n\t\treverse_proxy relief-observatoire:8774\n\t}\n',1)
    p.write_text(text)
print('Route /relief/* ajoutée ; autres handlers conservés.')
