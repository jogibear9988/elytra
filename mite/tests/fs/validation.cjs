const fs=require('node:fs');
function error(label,fn){try{const value=fn();console.log(label,typeof value==='object'?JSON.stringify(value):String(value));}catch(e){console.log(label,e.name,e.code);}}
for(const value of [null,1,{},[],new Uint8Array(3),'bad\0path'])error('path',()=>fs.readFileSync(value));
for(const value of [1,false,[],{},'bad',''])error('encoding',()=>fs.readFileSync('fixture.txt',value));
for(const value of [null,false,1,{},'invalid'])error('flag',()=>fs.writeFileSync('written',Buffer.from('x'),{flag:value}));
for(const value of [null,-1,1.5,'8','644'])error('mode',()=>fs.writeFileSync('written','x',{mode:value}));
for(const value of [1,null,{},[1,2]])error('data',()=>fs.writeFileSync('written',value));
for(const value of [false,null,0,''])error('read options',()=>fs.readFileSync('fixture.txt',{encoding:value,flag:value}));
error('callback',()=>fs.readFile('fixture.txt',null));
error('bad path callback',()=>fs.readFile(null,()=>{}));
error('recursive',()=>fs.mkdirSync('dir',{recursive:'yes'}));
error('missing stat',()=>fs.statSync('missing',{throwIfNoEntry:false}));
