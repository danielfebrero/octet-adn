const {execFileSync}=require('node:child_process');
for(const file of ['app.js','world.js'])execFileSync(process.execPath,['--check',__dirname+'/'+file],{stdio:'inherit'});
console.log('OK: syntaxe des interfaces 2D et 3D.');
