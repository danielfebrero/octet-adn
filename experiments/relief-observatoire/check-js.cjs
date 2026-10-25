const fs=require('fs'),vm=require('vm'),assert=require('assert');
const node=()=>({value:'0',hidden:false,classList:{toggle(){}},getContext(){return{};}}),nodes={};
const context=vm.createContext({window:{},document:{getElementById:id=>nodes[id]??=node(),querySelectorAll:()=>[]},localStorage:{getItem(){return null;}},setInterval(){},console});
vm.runInContext(fs.readFileSync(__dirname+'/app.js','utf8'),context);
vm.runInContext(`
for(const seed of [1n,20261025n,0xffffffffffffffffn]){
 let x=seed;
 for(let n=0n;n<300n;n++){
  if(jump(seed,n)!==x)throw Error('saut incorrect '+n);
  x=step(x);
 }
}
const a=20261025n,b=1000000000n,c=123456789n;
if(jump(jump(a,b),c)!==jump(a,b+c))throw Error('composition');
`,context);
console.log('OK: JS analysé; saut GF(2) comparé à 900 pas directs et composition de grands sauts.');
