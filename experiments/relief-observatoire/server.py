"""Relief: read-only world mounted /world:ro. No world control endpoint."""
import hashlib, hmac, json, os, subprocess, time, threading, csv
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qsl, urlsplit, parse_qs

ROOT=Path(__file__).parent
TOKEN=os.environ.get('TELEGRAM_BOT_TOKEN','')
ALLOWED=set(os.environ.get('TELEGRAM_ALLOWED_USER_IDS','').split(','))
WORLD=os.environ.get('RELIEF_WORLD','/world')
LOCK=threading.Lock()
CACHE={}

def verify(raw):
    pairs=parse_qsl(raw,keep_blank_values=True,strict_parsing=True)
    if len({k for k,v in pairs})!=len(pairs): raise ValueError('duplicate')
    data=dict(pairs); received=data.pop('hash','')
    secret=hmac.new(b'WebAppData',TOKEN.encode(),hashlib.sha256).digest()
    calc=hmac.new(secret,'\n'.join(k+'='+v for k,v in sorted(data.items())).encode(),hashlib.sha256).hexdigest()
    if not TOKEN or not hmac.compare_digest(received,calc):raise ValueError('hash')
    age=time.time()-int(data['auth_date'])
    if age < -30 or age>86400:raise ValueError('expired')
    if str(json.loads(data['user'])['id']) not in ALLOWED:raise ValueError('user')

def snapshot(x,y,ident):
    stamp=Path(WORLD,'state.ck').stat().st_mtime_ns
    key=(stamp,x,y,ident)
    with LOCK:
        if key in CACHE:return CACHE[key]
        data=json.loads(subprocess.check_output([str(ROOT/'reader'),WORLD,str(x),str(y),str(ident)],timeout=12))
        data['saved_at']=Path(WORLD,'state.ck').stat().st_mtime
        data['read_at']=time.time()
        rows=[]
        # series is currently unrotated; include closed series segments when present.
        import gzip
        paths=sorted(Path(WORLD,'histoire').glob('series*.gz'))+[Path(WORLD,'series.tsv')]
        for path in paths:
            try:
                op=gzip.open if path.suffix=='.gz' else open
                with op(path,'rt') as f:
                    for row in csv.DictReader(f,delimiter='\t'):
                        try:r={k:int(v) for k,v in row.items()}
                        except (TypeError,ValueError):continue
                        if r['tick']<=data['tick']:rows.append(r)
            except FileNotFoundError:pass
        rows=sorted({r['tick']:r for r in rows}.values(),key=lambda r:r['tick'])
        stride=max(1,len(rows)//600)
        data['series']=rows[::stride]
        if rows and data['series'][-1]!=rows[-1]:data['series'].append(rows[-1])
        data['source']='ELF005 checkpoint; lecteur séparé; aucune écriture sur le vivant'
        if len(CACHE)>10:CACHE.clear()
        CACHE[key]=data
        return data

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def send(self,status,data,kind='application/json; charset=utf-8'):
        raw=data if isinstance(data,bytes) else json.dumps(data,ensure_ascii=False).encode()
        self.send_response(status);self.send_header('Content-Type',kind)
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
    def do_GET(self):
        path=urlsplit(self.path).path
        files={'/relief/':'index.html','/relief/app.js':'app.js','/relief/app.css':'app.css','/relief/telegram-web-app.js':'telegram-web-app.js'}
        if path=='/health':return self.send(200,{'ok':True,'readonly':True})
        if path in files:
            name=files[path];kind='text/html' if name.endswith('.html') else 'text/css' if name.endswith('.css') else 'text/javascript'
            return self.send(200,(ROOT/name).read_bytes(),kind+'; charset=utf-8')
        self.send(404,{'error':'introuvable'})
    def do_POST(self):
        if urlsplit(self.path).path!='/relief/data':return self.send(404,{})
        try:
            n=int(self.headers.get('Content-Length','0'))
            if n<1 or n>16384:raise ValueError('size')
            payload=json.loads(self.rfile.read(n));verify(payload.get('init_data',''))
        except Exception:return self.send(403,{'error':'Ouvrir depuis le bouton Telegram Relief.'})
        try:
            x=max(0,min(1023,int(payload.get('x',512))));y=max(0,min(511,int(payload.get('y',256))))
            ident=int(payload.get('id',0))
            if ident<0 or ident>2**64-1:raise ValueError('id')
            self.send(200,snapshot(x,y,ident))
        except Exception:self.send(503,{'error':'Lecture du checkpoint indisponible. Réessayer.'})

if __name__=='__main__':ThreadingHTTPServer(('0.0.0.0',8774),Handler).serve_forever()
