const fs=require('node:fs');const p=require('node:fs/promises');
console.log(fs===require('fs'),p===require('fs/promises'),p===fs.promises);
const original=Buffer;
globalThis.Buffer=function Replacement(){};
console.log(original.isBuffer(fs.readFileSync('fixture.txt')));
import('node:fs').then(async mod=>{
    console.log(mod.default===fs,mod.readFile===fs.readFile,(await import('node:fs/promises')).default===p);
});
