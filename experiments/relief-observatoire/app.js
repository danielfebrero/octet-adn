'use strict';
const $=id=>document.getElementById(id),tg=window.Telegram?.WebApp;
tg?.ready();tg?.expand();
// Keep camera controls at the top with the main navigation.
const worldTools=document.querySelector('.world-tools');
if(worldTools) $('world').insertBefore(worldTools,$('world').firstChild);
let d=null,busy=false,sel={x:512,y:256,id:'0'},history=[];
try{sel=JSON.parse(localStorage.getItem('relief.selection'))||sel;$('layer').value=localStorage.getItem('relief.layer')||'0';}catch{}
const fmt=n=>Number(n).toLocaleString('fr-FR'),ops=['manger','avancer','orienter','croître','se reproduire','construire','défaire','déposer','reprendre','attaquer','parasiter','donner','émettre','percevoir','conditionner','attendre'];
function tab(name){for(const s of ['world','obs','micro'])$(s).hidden=s!==name;document.querySelectorAll('[data-tab]').forEach(b=>b.classList.toggle('active',b.dataset.tab===name));}
document.querySelectorAll('[data-tab]').forEach(b=>b.onclick=()=>tab(b.dataset.tab));tab('world');
function remember(){localStorage.setItem('relief.selection',JSON.stringify(sel));localStorage.setItem('relief.layer',$('layer').value);}
async function load(){
 if(busy||!tg?.initData)return;busy=true;$('err').textContent='';
 try{const r=await fetch('data',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({init_data:tg.initData,...sel})});const out=await r.json();if(!r.ok)throw Error(out.error);d=out;render();remember();}
 catch(e){$('err').textContent=e.message;}finally{busy=false;}
}
function render(){
 $('status').textContent=`Tick ${fmt(d.tick)} · sauvegarde ${new Date(d.saved_at*1000).toLocaleString('fr-FR',{timeZone:'UTC'})} UTC · ${Math.round(Date.now()/1000-d.saved_at)} s avant maintenant`;
 const last=d.series.at(-1)||{},prev=d.series.at(-2),rows=[['Organismes',fmt(d.population)],['Lignées fondatrices',fmt(last.lineages||0)],['Empreintes génétiques',fmt(last.genotypes_hash||0)],['Matière conservée',d.matter===d.initial_matter?'Oui · '+fmt(d.matter):'ALERTE']];
 $('cards').innerHTML=rows.map(([k,v])=>`<div class="card"><strong>${v}</strong><span>${k}</span></div>`).join('');
 if(prev) $('cards').innerHTML+=`<div class="card"><strong>${fmt(last.births-prev.births)}</strong><span>Naissances / ${last.tick-prev.tick} ticks</span></div><div class="card"><strong>${fmt(last.deaths-prev.deaths)}</strong><span>Décès / même fenêtre</span></div>`;
 drawMap();chart();micro();window.dispatchEvent(new CustomEvent('relief-data',{detail:d}));
}
function drawMap(){if(!d)return;const ctx=$('map').getContext('2d'),img=ctx.createImageData(128,64),k=+$('layer').value,max=Math.max(1,...d.map.map(a=>a[k]));d.map.forEach((a,i)=>{const t=Math.log1p(a[k])/Math.log1p(max);img.data.set([Math.round(20+170*t),Math.round(32+190*t),Math.round(38+90*t),255],i*4);});ctx.putImageData(img,0,0);}
$('layer').onchange=()=>{drawMap();remember();};
$('map').onclick=e=>{const r=e.target.getBoundingClientRect();sel={x:Math.min(1023,Math.floor((e.clientX-r.left)/r.width*1024)),y:Math.min(511,Math.floor((e.clientY-r.top)/r.height*512)),id:'0'};tab('micro');load();};
function chart(){if(!d)return;const c=$('chart'),ctx=c.getContext('2d'),key=$('metric').value,rows=d.series;if(!rows.length)return;const max=Math.max(1,...rows.map(r=>r[key])),min=Math.min(...rows.map(r=>r[key])),first=rows[0].tick,last=rows.at(-1).tick;ctx.clearRect(0,0,c.width,c.height);ctx.strokeStyle='#b3d593';ctx.lineWidth=2;ctx.beginPath();rows.forEach((r,i)=>{const x=12+(r.tick-first)/(last-first||1)*(c.width-24),y=c.height-15-(r[key]-min)/(max-min||1)*(c.height-30);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});ctx.stroke();$('range').textContent=`Ticks ${fmt(first)} → ${fmt(last)} · min ${fmt(min)} · max ${fmt(max)} · dernier ${fmt(rows.at(-1)[key])}`;}
$('metric').onchange=chart;
function micro(){
 $('zone').textContent=`Zone autour de (${d.x}, ${d.y}) · tick ${fmt(d.tick)}`;
 const ctx=$('zoom').getContext('2d');ctx.clearRect(0,0,340,340);d.nearby.filter(c=>Math.abs(c[0]-d.x)<=8&&Math.abs(c[1]-d.y)<=8).forEach(c=>{const x=(c[0]-d.x+8)*20,y=(c[1]-d.y+8)*20;ctx.fillStyle=c[4]?'#62666b':c[5]!=='0'?'#a7cf85':`rgb(20,${Math.min(120,35+c[2])},55)`;ctx.fillRect(x,y,19,19);if(d.agent&&c[5]===d.agent.id){ctx.strokeStyle='white';ctx.strokeRect(x,y,19,19);}});
 const a=d.agent;if(!a){$('agent').textContent=sel.id!=='0'?`Organisme ${sel.id} absent de cette sauvegarde (décédé ou identifiant inexistant).`:'Aucun organisme dans cette zone.';return;}
 if(sel.id!=='0')$('ident').value=sel.id;
 const fields=[['Identité',a.id],['Parent / lignée',a.parent+' / '+a.lineage],['Position / direction',`${a.x}, ${a.y} / ${['est','sud','ouest','nord'][a.dir]}`],['Âge / naissance',`${a.age} ticks / tick ${a.born}`],['Énergie / corps / réserve',`${a.energy} / ${a.body} / ${a.store}`],['Mémoire (8 octets)',a.memory.join(' · ')],['Perceptions actuelles',a.sensors.join(' · ')],['Choix décodé',`${ops[a.choice&15]} · argument ${a.choice>>4} · ${a.steps} instructions`],['Sans perceptions / sans mémoire',`${ops[a.no_sensors&15]} / ${ops[a.no_memory&15]}`]];
 $('agent').innerHTML='<table>'+fields.map(([k,v])=>`<tr><th>${k}</th><td>${v}</td></tr>`).join('')+'</table><p class="hint">Perceptions : nourriture ici, énergie, réserve, nourriture devant, occupant devant, humidité, signal, abri. Valeurs codées 0–255.</p><h3>Génome · 64 octets</h3><div class="genome">'+a.genome.map((v,i)=>`<span class="gene ${i===a.pc?'current':''}" title="${i} : ${ops[v&15]}, argument ${v>>4}">${v.toString(16).padStart(2,'0')}</span>`).join('')+'</div><h3>Lecture du programme</h3><pre id="trace"></pre>';
 let pc=a.pc,mem=[...a.memory],lines=[];
 for(let j=0;j<8;j++){const pos=pc,v=a.genome[pc],op=v&15,arg=v>>4;pc=(pc+1)%64;let note='';if(op===13){mem[arg&7]=a.sensors[arg&7];note=` → mémoire[${arg&7}]=${mem[arg&7]}`;}else if(op===14){const yes=mem[arg&7]<(arg<8?64:192);if(yes)pc=(pc+1)%64;note=yes?' → saut du prochain octet':' → pas de saut';}lines.push(`${pos}: ${ops[op]} (${arg})${note}`);if(op!==13&&op!==14)break;}
 $('trace').textContent=lines.join('\n');
 if(!history.some(r=>r.id===a.id&&r.tick===d.tick)){history.push({id:a.id,tick:d.tick,age:a.age,energy:a.energy,pos:`${a.x},${a.y}`});history=history.slice(-40);}
 $('trail').textContent=history.filter(r=>r.id===a.id).slice(-6).map(r=>`t${r.tick} : (${r.pos}), âge ${r.age}, énergie ${r.energy}`).join(' · ');
}
$('zoom').onclick=e=>{const r=e.target.getBoundingClientRect(),x=d.x-8+Math.floor((e.clientX-r.left)/r.width*17),y=d.y-8+Math.floor((e.clientY-r.top)/r.height*17),c=d.nearby.find(c=>c[0]===x&&c[1]===y);if(c&&c[5]!=='0'){sel.id=c[5];load();}};
$('search').onsubmit=e=>{e.preventDefault();if(/^\d{1,20}$/.test($('ident').value)){sel.id=$('ident').value;load();}};
$('untrack').onclick=()=>{sel.id='0';$('ident').value='';load();};
$('refresh').onclick=load;setInterval(()=>{if($('auto').checked&&!document.hidden)load();},15000);
load();

window.reliefSelect=(id,x,y,inspect=false)=>{sel={id:String(id||'0'),x:Math.round(x),y:Math.round(y)};if(inspect)tab('micro');load();};
window.reliefData=()=>d;
