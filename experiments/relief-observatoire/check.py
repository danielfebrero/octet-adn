"""Integration checks; run inside service container, no credentials printed."""
import hashlib,hmac,json,time,urllib.request,urllib.error
import server

def signed(uid,age=0):
    fields={'auth_date':str(int(time.time())-age),'user':json.dumps({'id':uid})}
    secret=hmac.new(b'WebAppData',server.TOKEN.encode(),hashlib.sha256).digest()
    fields['hash']=hmac.new(secret,'\n'.join(k+'='+v for k,v in sorted(fields.items())).encode(),hashlib.sha256).hexdigest()
    from urllib.parse import urlencode
    return urlencode(fields)

def request(raw,expected,**values):
    payload={'init_data':raw,**values}
    req=urllib.request.Request('http://127.0.0.1:8774/relief/data',data=json.dumps(payload).encode(),headers={'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req) as r:status=r.status;data=json.load(r)
    except urllib.error.HTTPError as e:status=e.code;data=json.load(e)
    assert status==expected,(status,expected)
    return data

uid=int(next(iter(server.ALLOWED)))
request('',403);request(signed(uid+1),403);request(signed(uid,90000),403)
raw=signed(uid);d=request(raw,200)
assert d['matter']==d['initial_matter']==76840404
assert len(d['map'])==8192 and d['series'] and d['agent']
a=d['agent'];next_d=request(raw,200,id=a['id'])
assert next_d['agent']['id']==a['id']
assert len(a['genome'])==64 and len(a['memory'])==8
assert len(a['sensors'])==8
print('OK: auth refusée sans signature, mauvais compte et expiration; carte, matière, séries, sélection et microscope. Tick',d['tick'])

organisms=d['organisms']
assert len(organisms)==d['population'] and len({a[0] for a in organisms})==len(organisms)
assert all(0<=a[1]<d['width'] and 0<=a[2]<d['height'] for a in organisms)
assert all(e['tick']<=d['tick'] for e in d['events'])
ids={a[0] for a in organisms}
assert all(c[5]=='0' or c[5] in ids for c in d['nearby'])
for name in ('','world.js','three.module.js','three.core.js','OrbitControls.js'):
    with urllib.request.urlopen('http://127.0.0.1:8774/relief/'+name) as r:
        assert r.status==200
        if name=='':assert 'Hasard' not in r.read().decode()
req=urllib.request.Request('http://127.0.0.1:8774/relief/data',data=json.dumps({'init_data':raw}).encode(),headers={'Content-Type':'application/json','Accept-Encoding':'gzip'})
import gzip
with urllib.request.urlopen(req) as r:
    assert r.headers['Content-Encoding']=='gzip'
    assert json.loads(gzip.decompress(r.read()))['population']==d['population']
print('OK: population 3D complète, identités uniques, coordonnées, loupe cohérente, événements datés, modules locaux, Hasard retiré et transfert gzip.')
