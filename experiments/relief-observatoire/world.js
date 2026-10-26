import * as T from './three.module.js';
import {OrbitControls} from './OrbitControls.js';
const $=id=>document.getElementById(id), fmt=n=>Number(n).toLocaleString('fr-FR');
const kinds={birth:['Naissance','#ade4a0'],death:['Décès','#99a4c5'],build:['Construction','#edbd78'],deposit:['Dépôt','#e9d692'],attack:['Attaque','#ef8b83'],gift:['Don','#91e3df'],parasite:['Parasitisme','#bf9bea']};
let data,renderer,scene,camera,controls,terrain,organisms,buildings,rocks,previous=new Map(),transition=0,focus=null,follow=false,animated=true,auto=false,lastFrame=0,started=false,ripple,focusRing,bodies;
const color=new T.Color(), dummy=new T.Object3D(), ray=new T.Raycaster(),pointer=new T.Vector2(), target=new T.Vector3();
const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;animated=!reduced;
let heightScale=0.12,fitted=false;
let echoes=[];const echoGeometry=new T.RingGeometry(0.75,1,24);
function altitude(x,y){if(!data)return 0;const bx=Math.max(0,Math.min(127,Math.floor(x/8))),by=Math.max(0,Math.min(63,Math.floor(y/8)));return data.map[by*128+bx][2]*heightScale;}
function lineageColor(id){return color.setHSL((Number(BigInt(id)%997n)*0.61803398875)%1,0.57,0.68);}
function dispose(obj){if(!obj)return;scene.remove(obj);obj.geometry.dispose();obj.dispose?.();if(Array.isArray(obj.material))obj.material.forEach(m=>m.dispose());else obj.material.dispose();}
function init(){
 try{
 renderer=new T.WebGLRenderer({antialias:true,alpha:false,powerPreference:'low-power'});renderer.setPixelRatio(Math.min(devicePixelRatio,1.5));renderer.setClearColor('#07151d');renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.ACESFilmicToneMapping;renderer.toneMappingExposure=1.22;
 $('scene').append(renderer.domElement);renderer.domElement.setAttribute('aria-label','Relief en trois dimensions, organismes et territoire réels');
 scene=new T.Scene();scene.fog=new T.FogExp2('#07151d',0.00032);camera=new T.PerspectiveCamera(47,1,0.3,3500);camera.position.set(730,700,920);
 controls=new OrbitControls(camera,renderer.domElement);controls.target.set(0,25,0);controls.enableDamping=true;controls.dampingFactor=0.065;controls.minDistance=9;controls.maxDistance=2800;controls.maxPolarAngle=Math.PI*0.47;controls.autoRotateSpeed=0.32;controls.enablePan=true;controls.screenSpacePanning=false;
 controls.addEventListener('start',()=>{$('scene-title')?.classList.add('quiet');});
 scene.add(new T.HemisphereLight('#b7ecff','#304638',1.6));const sun=new T.DirectionalLight('#ffdbab',2.3);sun.position.set(-300,600,100);scene.add(sun);const rim=new T.DirectionalLight('#72ced4',1.0);rim.position.set(300,100,-400);scene.add(rim);
 const slab=new T.Mesh(new T.BoxGeometry(1024,28,512),new T.MeshStandardMaterial({color:'#0b292e',roughness:0.9}));slab.position.y=-15;scene.add(slab);
 const grid=new T.GridHelper(1600,40,'#183c48','#112b38');grid.position.y=-30;scene.add(grid);
 focusRing=new T.Mesh(new T.TorusGeometry(2,0.12,6,48),new T.MeshBasicMaterial({color:'#ffffff'}));focusRing.rotation.x=Math.PI/2;focusRing.visible=false;scene.add(focusRing);
 ripple=new T.Mesh(new T.RingGeometry(0.8,1,48),new T.MeshBasicMaterial({color:'#9ae9db',transparent:true,opacity:0,side:T.DoubleSide,depthWrite:false}));ripple.rotation.x=-Math.PI/2;scene.add(ripple);
 const ro=new ResizeObserver(()=>{const r=$('scene').getBoundingClientRect();if(r.width&&r.height){renderer.setSize(r.width,r.height,false);camera.aspect=r.width/r.height;camera.updateProjectionMatrix();if(!fitted){home();fitted=true;}}});ro.observe($('scene'));
 let down;renderer.domElement.addEventListener('pointerdown',e=>down={x:e.clientX,y:e.clientY});renderer.domElement.addEventListener('pointerup',e=>{if(down&&Math.hypot(e.clientX-down.x,e.clientY-down.y)<6)pick(e);down=null;});
 renderer.domElement.addEventListener('webglcontextlost',e=>{e.preventDefault();$('scene-loading').hidden=false;$('scene-loading').textContent='La vue 3D a été suspendue. Rouvrez Relief ; la carte 2D reste accessible.';});
 started=true;$('scene-loading').textContent='En attente des données Telegram…';requestAnimationFrame(frame);
 }catch(e){$('scene-loading').textContent='3D indisponible sur cet appareil. L’Observatoire et le microscope restent accessibles.';console.error(e);}
}
function makeTerrain(){
 dispose(terrain);dispose(buildings);dispose(rocks);
 const g=new T.PlaneGeometry(1024,512,127,63);g.rotateX(-Math.PI/2);const pos=g.attributes.position,colors=new Float32Array(pos.count*3),mode=$('world-layer').value;
 for(let i=0;i<pos.count;i++){
  const a=data.map[i],v=mode==='food'?a[1]:mode==='structure'?a[3]:a[2];pos.setY(i,a[2]*heightScale);
  const f=Math.min(1,Math.log1p(v)/Math.log1p(mode==='food'?120:mode==='structure'?100:1024));
  color.setHSL(mode==='food'?0.24:mode==='structure'?0.09:0.47-f*0.26,mode==='structure'?0.42:0.26,0.12+f*0.25);
  if(a[4]>24)color.lerp(new T.Color('#89969b'),a[4]/100);color.toArray(colors,i*3);
 }
 g.setAttribute('color',new T.BufferAttribute(colors,3));g.computeVertexNormals();terrain=new T.Mesh(g,new T.MeshStandardMaterial({vertexColors:true,roughness:0.94,flatShading:true,side:T.DoubleSide}));scene.add(terrain);
 const built=data.map.map((a,i)=>({a,i})).filter(({a})=>a[3]>0),rocky=data.map.map((a,i)=>({a,i})).filter(({a})=>a[4]>20);
 buildings=new T.InstancedMesh(new T.BoxGeometry(1,1,1),new T.MeshStandardMaterial({color:'#c6aa71',emissive:'#64421b',emissiveIntensity:0.2,roughness:0.65}),Math.max(1,built.length));buildings.count=built.length;
 built.forEach(({a,i},j)=>{const h=Math.min(16,1+Math.log1p(a[3])*2);dummy.position.set(i%128*8-508,a[2]*heightScale+h/2,Math.floor(i/128)*8-252);dummy.scale.set(2.8,h,2.8);dummy.rotation.set(0,0,0);dummy.updateMatrix();buildings.setMatrixAt(j,dummy.matrix);});scene.add(buildings);
 rocks=new T.InstancedMesh(new T.OctahedronGeometry(1,0),new T.MeshStandardMaterial({color:'#76918e',roughness:1,flatShading:true}),Math.max(1,rocky.length));rocks.count=rocky.length;
 rocky.forEach(({a,i},j)=>{dummy.position.set(i%128*8-508,a[2]*heightScale+2,Math.floor(i/128)*8-252);dummy.scale.set(3,3+a[4]/20,3);dummy.rotation.set(0,i*0.7,0);dummy.updateMatrix();rocks.setMatrixAt(j,dummy.matrix);});scene.add(rocks);
}
function makeBodies(){
 dispose(bodies);const rows=live.on?Array.from({length:live.cap},(_,i)=>i).filter(i=>live.id[i]&&Math.abs(live.x[i]-data.x)<=24&&Math.abs(live.y[i]-data.y)<=24).map(liveRow):data.organisms.filter(a=>Math.abs(a[1]-data.x)<=24&&Math.abs(a[2]-data.y)<=24);
 bodies=new T.InstancedMesh(new T.CapsuleGeometry(0.4,0.7,2,5),new T.MeshStandardMaterial({roughness:0.3,metalness:0.2,emissive:'#6aa69a',emissiveIntensity:0.18}),Math.max(1,rows.length));bodies.count=rows.length;
 rows.forEach((a,i)=>{const size=Math.min(1.4,0.55+Math.sqrt(a[6])*0.06);dummy.position.set(a[1]-512,altitude(a[1],a[2])+1,a[2]-256);dummy.scale.set(size,size,size);dummy.rotation.set(Math.PI/2,0,Math.PI/2-a[4]*Math.PI/2);dummy.updateMatrix();bodies.setMatrixAt(i,dummy.matrix);bodies.setColorAt(i,$('world-layer').value==='energy'?color.setHSL(0.02+Math.min(1,a[5]/1000)*0.4,0.7,0.68):lineageColor(a[3]));});scene.add(bodies);
}
function makeOrganisms(now){if(live.on){liveRefresh();return;}
 const old=previous;previous=new Map();const rows=data.organisms,positions=new Float32Array(rows.length*3),starts=new Float32Array(rows.length*3),colors=new Float32Array(rows.length*3),seeds=new Float32Array(rows.length);
 rows.forEach((a,i)=>{const end=[a[1]-512,altitude(a[1],a[2])+1.8,a[2]-256],p=old.get(a[0]);positions.set(end,i*3);starts.set(p&&Math.hypot(end[0]-p[0],end[2]-p[2])<40?p:end,i*3);previous.set(a[0],end);($('world-layer').value==='energy'?color.setHSL(0.02+Math.min(1,a[5]/1000)*0.4,0.7,0.68):lineageColor(a[3])).toArray(colors,i*3);seeds[i]=Number(BigInt(a[0])%1000n)/1000;});
 dispose(organisms);const g=new T.BufferGeometry();g.setAttribute('position',new T.BufferAttribute(positions,3));g.setAttribute('start',new T.BufferAttribute(starts,3));g.setAttribute('color',new T.BufferAttribute(colors,3));g.setAttribute('seed',new T.BufferAttribute(seeds,1));
 const m=new T.ShaderMaterial({transparent:true,depthWrite:false,vertexColors:true,uniforms:{progress:{value:1},time:{value:0},pixelRatio:{value:renderer.getPixelRatio()},animate:{value:animated?1:0}},vertexShader:`attribute vec3 start;attribute float seed;varying vec3 vColor;varying float vSeed;uniform float progress;uniform float pixelRatio;void main(){vColor=color;vSeed=seed;vec3 p=mix(start,position,progress);vec4 mv=modelViewMatrix*vec4(p,1.);gl_Position=projectionMatrix*mv;gl_PointSize=clamp(2000.0/max(5.,-mv.z),1.5,24.)*pixelRatio;}`,fragmentShader:`varying vec3 vColor;varying float vSeed;uniform float time;uniform float animate;void main(){float r=length(gl_PointCoord-.5)*2.;if(r>1.)discard;float glow=pow(1.-r,1.4);float core=1.-smoothstep(.1,.45,r);float pulse=1.+animate*.12*sin(time*1.8+vSeed*6.28);gl_FragColor=vec4(vColor*pulse+vec3(core*.35),glow*.85+core*.15);}`});
 organisms=new T.Points(g,m);organisms.frustumCulled=false;scene.add(organisms);transition=now;
}
function receive(d){const changed=!data||d.tick!==data.tick;data=d;if(!started)return;
 $('scene-loading').hidden=true;$('scene-count').textContent=fmt(d.population)+' octets';if(!live.on)$('scene-tick').textContent='t '+fmt(d.tick);
 if(changed){makeTerrain();makeOrganisms(performance.now());renderEvents();makeEchoes();}makeBodies();
 if(focus&&!live.on){const a=data.organisms.find(a=>a[0]===focus.id);if(a){focus.row=a;focus.x=a[1];focus.y=a[2];}else{focus.row=null;follow=false;}$('focus-info').textContent=focus.row?`Lignée ${focus.row[3]} · énergie ${fmt(focus.row[5])} · (${focus.x}, ${focus.y})`:'Absent de cette sauvegarde · suivi interrompu';}
}
/* ---------- Direct : ombre déterministe servie par /relief/live (trames K, D, V) ---------- */
const live={on:false,tick:-1,cap:0,id:null,x:null,y:null,lin:null,en:null,at:0,dur:500,last:0,busy:false,verified:null,fails:0};
function liveColor(i){return $('world-layer').value==='energy'?color.setHSL(0.02+Math.min(1,live.en[i]/1000)*0.4,0.7,0.68):lineageColor(String(live.lin[i]));}
function livePlace(i,arr){const x=live.x[i],y=live.y[i];if(!live.id[i]){arr[i*3]=0;arr[i*3+1]=-2000;arr[i*3+2]=0;return;}arr[i*3]=x-512;arr[i*3+1]=altitude(x,y)+1.8;arr[i*3+2]=y-256;}
function liveRow(i){return [String(live.id[i]),live.x[i],live.y[i],String(live.lin[i]),0,live.en[i],0];}
function liveRefresh(){if(!live.on||!organisms)return;const g=organisms.geometry,p=g.attributes.position.array,s=g.attributes.start.array,c=g.attributes.color.array;for(let i=0;i<live.cap;i++){livePlace(i,p);liveColor(i).toArray(c,i*3);}s.set(p);g.attributes.position.needsUpdate=g.attributes.start.needsUpdate=g.attributes.color.needsUpdate=true;}
function liveKey(dv,o,tick){
 const cap=dv.getUint32(o,true);o+=4;live.cap=cap;live.id=new Uint32Array(cap);live.x=new Uint16Array(cap);live.y=new Uint16Array(cap);live.lin=new Uint16Array(cap);live.en=new Uint16Array(cap);
 for(let i=0;i<cap;i++,o+=12){live.id[i]=dv.getUint32(o,true);live.x[i]=dv.getUint16(o+4,true);live.y[i]=dv.getUint16(o+6,true);live.lin[i]=dv.getUint16(o+8,true);live.en[i]=dv.getUint16(o+10,true);}
 const positions=new Float32Array(cap*3),colors=new Float32Array(cap*3),seeds=new Float32Array(cap);
 for(let i=0;i<cap;i++){livePlace(i,positions);liveColor(i).toArray(colors,i*3);seeds[i]=(i*0.61803398875)%1;}
 dispose(organisms);const g=new T.BufferGeometry();g.setAttribute('position',new T.BufferAttribute(positions,3));g.setAttribute('start',new T.BufferAttribute(positions.slice(),3));g.setAttribute('color',new T.BufferAttribute(colors,3));g.setAttribute('seed',new T.BufferAttribute(seeds,1));
 const m=new T.ShaderMaterial({transparent:true,depthWrite:false,vertexColors:true,uniforms:{progress:{value:1},time:{value:0},pixelRatio:{value:renderer.getPixelRatio()},animate:{value:animated?1:0}},vertexShader:`attribute vec3 start;attribute float seed;varying vec3 vColor;varying float vSeed;uniform float progress;uniform float pixelRatio;void main(){vColor=color;vSeed=seed;vec3 p=mix(start,position,progress);if(position.y<-1000.){gl_Position=vec4(2.,2.,2.,1.);gl_PointSize=0.;return;}vec4 mv=modelViewMatrix*vec4(p,1.);gl_Position=projectionMatrix*mv;gl_PointSize=clamp(2000.0/max(5.,-mv.z),1.5,24.)*pixelRatio;}`,fragmentShader:`varying vec3 vColor;varying float vSeed;uniform float time;uniform float animate;void main(){float r=length(gl_PointCoord-.5)*2.;if(r>1.)discard;float glow=pow(1.-r,1.4);float core=1.-smoothstep(.1,.45,r);float pulse=1.+animate*.12*sin(time*1.8+vSeed*6.28);gl_FragColor=vec4(vColor*pulse+vec3(core*.35),glow*.85+core*.15);}`});
 organisms=new T.Points(g,m);organisms.frustumCulled=false;scene.add(organisms);
 live.on=true;live.tick=tick;live.at=performance.now();live.last=live.at;
}
function liveDelta(dv,o,tick){
 const from=Number(dv.getBigUint64(o,true));if(from!==live.tick)return false;
 const nd=dv.getUint32(o+8,true),nb=dv.getUint32(o+12,true),nj=dv.getUint32(o+16,true);o+=20;
 const g=organisms.geometry,p=g.attributes.position.array,s=g.attributes.start.array,c=g.attributes.color.array;s.set(p);
 for(let k=0;k<nd;k++,o+=2){const i=dv.getUint16(o,true);live.id[i]=0;livePlace(i,p);livePlace(i,s);}
 for(let k=0;k<nb;k++,o+=12){const i=dv.getUint16(o,true);live.x[i]=dv.getUint16(o+2,true);live.y[i]=dv.getUint16(o+4,true);live.lin[i]=dv.getUint16(o+6,true);live.id[i]=dv.getUint32(o+8,true);live.en[i]=500;livePlace(i,p);livePlace(i,s);liveColor(i).toArray(c,i*3);}
 const jumped=new Set();for(let k=0;k<nj;k++,o+=6){const i=dv.getUint16(o,true);live.x[i]=dv.getUint16(o+2,true);live.y[i]=dv.getUint16(o+4,true);livePlace(i,p);livePlace(i,s);jumped.add(i);}
 for(let i=0;i<live.cap;i++){const m=dv.getUint8(o+i);if(m===0x88||!live.id[i]||jumped.has(i))continue;live.x[i]+=(m>>4)-8;live.y[i]+=(m&15)-8;livePlace(i,p);}
 g.attributes.position.needsUpdate=g.attributes.start.needsUpdate=g.attributes.color.needsUpdate=true;
 const now=performance.now();live.dur=Math.max(300,Math.min(1500,now-live.last));live.last=now;live.at=now;live.tick=tick;
 if(focus&&focus.slot!=null){if(live.id[focus.slot]===focus.num){focus.x=live.x[focus.slot];focus.y=live.y[focus.slot];focus.row=liveRow(focus.slot);$('focus-info').textContent=`Lignée ${focus.row[3]} · (${focus.x}, ${focus.y}) · en direct`;}else if(focus.row){focus.row=null;follow=false;$('focus-info').textContent='A quitté le monde (décès) · suivi interrompu';}}
 if(bodies&&camera.position.distanceTo(controls.target)<180)makeBodies();
 return true;
}
function liveApply(buf){
 const dv=new DataView(buf);let o=0,ok=true;
 while(o+20<=buf.byteLength){const n=dv.getUint32(o+8,true),tick=Number(dv.getBigUint64(o+12,true)),kind=String.fromCharCode(dv.getUint8(o+4));o+=20;
  if(kind==='K')liveKey(dv,o,tick);else if(kind==='D'&&live.on){if(!liveDelta(dv,o,tick)){ok=false;break;}}
  o+=n;}
 if(!ok)live.tick=-1;
 $('scene-tick').textContent=`● DIRECT · t ${fmt(live.tick)}`+(live.verified?` · vérifié ${live.verified[1]?'✓':'✗'} au tick ${fmt(live.verified[0])}`:'');
}
async function livePoll(){
 if(live.busy||!data||!started||document.hidden||$('world').hidden||!window.Telegram?.WebApp?.initData)return;live.busy=true;
 try{const r=await fetch('live',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({init_data:Telegram.WebApp.initData,since:live.on?live.tick:-1})});
  const v=r.headers.get('X-Relief-Verified');if(v){const [t,k]=v.split(':');live.verified=[Number(t),k==='1'];}
  if(r.status===200){const b=await r.arrayBuffer();if(b.byteLength)liveApply(b);live.fails=0;}else if(r.status!==202)live.fails++;
 }catch(e){live.fails++;}finally{live.busy=false;}
 if(live.fails>20&&live.on){live.on=false;live.tick=-1;makeOrganisms(performance.now());}
}
setInterval(livePoll,450);
function recentDiverse(limit=20){const counts={};return [...(data.events||[])].reverse().filter(e=>{const n=counts[e.kind]||0;counts[e.kind]=n+1;return n<4;}).slice(0,limit);}
function makeEchoes(){for(const e of echoes){scene.remove(e.mesh);e.mesh.material.dispose();}echoes=recentDiverse().map((e,i)=>{const mesh=new T.Mesh(echoGeometry,new T.MeshBasicMaterial({color:(kinds[e.kind]||['','#a9d6d0'])[1],transparent:true,opacity:0,depthWrite:false,side:T.DoubleSide}));mesh.rotation.x=-Math.PI/2;mesh.position.set(e.x-512,altitude(e.x,e.y)+1,e.y-256);scene.add(mesh);return {mesh,delay:i*1.7};});}
function renderEvents(){const events=recentDiverse();$('events').replaceChildren();if(!events.length){$('events').textContent='Aucune trace disponible dans la fenêtre récente.';return;}for(const e of events){const [label,ink]=kinds[e.kind]||[e.kind,'#9cbbc1'],b=document.createElement('button');b.className='event';b.style.setProperty('--event-color',ink);const icon=document.createElement('span');icon.className='event-icon';icon.textContent=e.kind==='birth'?'+':e.kind==='death'?'−':'◈';const text=document.createElement('span');text.textContent=`${label} · octet ${e.id}`;const small=document.createElement('small');small.textContent=`t ${fmt(e.tick)} · ${e.x}, ${e.y}`;b.append(icon,text,small);b.onclick=()=>{visit(e.x,e.y,45);ripple.position.set(e.x-512,altitude(e.x,e.y)+0.8,e.y-256);ripple.userData.at=performance.now();ripple.material.color.set(ink);};$('events').append(b);}}
function pick(e){if(!data||!organisms)return;const rect=renderer.domElement.getBoundingClientRect();pointer.set((e.clientX-rect.left)/rect.width*2-1,-(e.clientY-rect.top)/rect.height*2+1);ray.setFromCamera(pointer,camera);ray.params.Points.threshold=Math.max(0.7,camera.position.distanceTo(controls.target)*0.004);const hit=ray.intersectObject(organisms)[0];if(hit&&(!live.on||live.id[hit.index])){const a=live.on?liveRow(hit.index):data.organisms[hit.index];focus={id:a[0],x:a[1],y:a[2],row:a,slot:live.on?hit.index:null,num:live.on?live.id[hit.index]:0};$('focus-card').hidden=false;$('focus-name').textContent='OCTET '+a[0];$('focus-info').textContent=`Lignée ${a[3]} · énergie ${fmt(a[5])} · (${a[1]}, ${a[2]})`;window.reliefSelect(a[0],a[1],a[2]);}else{const hit=ray.intersectObject(terrain)[0];if(hit){const x=Math.max(0,Math.min(1023,Math.round(hit.point.x+512))),y=Math.max(0,Math.min(511,Math.round(hit.point.z+256)));visit(x,y,80);window.reliefSelect('0',x,y);}}}
function home(){const k=camera.aspect<1?1.58:1;controls.target.set(0,25,0);camera.position.set(730*k,700*k,920*k);controls.update();}
function visit(x,y,distance=80){target.set(x-512,altitude(x,y),y-256);controls.target.copy(target);camera.position.copy(target).add(new T.Vector3(distance*0.65,distance*0.9,distance));let high=target.y;for(let dx=-distance;dx<=distance;dx+=8)for(let dy=-distance;dy<=distance;dy+=8)high=Math.max(high,altitude(x+dx,y+dy));camera.position.y=Math.max(camera.position.y,high+18);controls.update();$('scene-title').classList.add('quiet');$('camera-label').textContent=`IMMERSION / ${Math.round(x)} · ${Math.round(y)}`;}
function frame(now){requestAnimationFrame(frame);if(document.hidden||$('world').hidden||now-lastFrame<32)return;lastFrame=now;
 controls.autoRotate=auto&&animated;controls.update();if(bodies)bodies.visible=camera.position.distanceTo(controls.target)<180;if(organisms){organisms.material.uniforms.progress.value=live.on?(animated?Math.min(1,(now-live.at)/live.dur):1):animated?Math.min(1,(now-transition)/8000):1;organisms.material.uniforms.time.value=now/1000;organisms.material.uniforms.animate.value=animated?1:0;}
 if(focus?.row){focusRing.visible=true;focusRing.position.set(focus.x-512,altitude(focus.x,focus.y)+0.8,focus.y-256);if(follow){const next=focusRing.position.clone(),delta=next.clone().sub(controls.target).multiplyScalar(0.06);controls.target.add(delta);camera.position.add(delta);}}else focusRing.visible=false;
 if(ripple.userData.at){const t=(now-ripple.userData.at)/2400;ripple.scale.setScalar(1+t*14);ripple.material.opacity=Math.max(0,0.8*(1-t));}
 for(const e of echoes){const t=((now/1000+e.delay)%20)/3;e.mesh.visible=animated&&t<1;if(t<1){e.mesh.scale.setScalar(1+t*8);e.mesh.material.opacity=(1-t)*0.7;}}
 renderer.render(scene,camera);
}
$('panorama').onclick=()=>{if(!started)return;follow=false;home();$('scene-title').classList.remove('quiet');$('camera-label').textContent='ORBITE / VUE D’ENSEMBLE';};
$('ground').onclick=()=>{if(!data||!started)return;const a=focus?.row||data.organisms[Math.floor(data.organisms.length/2)];if(a){visit(a[1],a[2],32);window.reliefSelect(a[0],a[1],a[2]);}};
$('orbit').onclick=()=>{auto=!auto;$('orbit').classList.toggle('active',auto);};
$('follow3d').onclick=()=>{if(!focus)return;follow=!follow;$('follow3d').textContent=follow?'Suivi actif':'Suivre';if(follow)visit(focus.x,focus.y,24);};
$('inspect3d').onclick=()=>{if(focus)window.reliefSelect(focus.id,focus.x,focus.y,true);};
$('closefocus').onclick=()=>{focus=null;follow=false;$('focus-card').hidden=true;};
$('world-layer').onchange=()=>{if(data&&started){makeTerrain();makeOrganisms(performance.now());makeBodies();}};
$('height-scale').oninput=e=>{heightScale=Number(e.target.value);if(data&&started){previous.clear();makeTerrain();makeOrganisms(performance.now());makeBodies();}};
$('full3d').onclick=()=>{$('scene-wrap').classList.toggle('expanded');$('full3d').textContent=$('scene-wrap').classList.contains('expanded')?'Réduire':'⛶';};
$('motion').onclick=()=>{animated=!animated;$('motion').textContent='Animations : '+(animated?'oui':'non');};$('motion').textContent='Animations : '+(animated?'oui':'non');
window.addEventListener('relief-data',e=>receive(e.detail));init();if(window.reliefData())receive(window.reliefData());
