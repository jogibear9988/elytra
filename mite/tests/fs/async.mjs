import fs, {readFile, writeFile} from 'node:fs';
import promises, {readFile as readPromise} from 'node:fs/promises';
console.log(fs.promises===promises,promises.readFile===readPromise,fs.constants===promises.constants);
const events=[];
await new Promise((resolve,reject)=>{
    events.push('start');
    writeFile('async.txt','async é',error=>{
        if(error) {reject(error);return;}
        events.push('write');queueMicrotask(()=>events.push('microtask'));
        readFile('async.txt','utf8',(error,data)=>{
            if(error){reject(error);return;}
            events.push(data);resolve();
        });
    });
    events.push('scheduled');queueMicrotask(()=>events.push('initial microtask'));
});
console.log(events.join(','));
await promises.appendFile('async.txt',' tail');
console.log((await promises.readFile('async.txt')).toString());
console.log(await promises.mkdir('async-dir/child',{recursive:true}));
console.log((await promises.stat('async.txt')).isFile(),(await promises.lstat('link')).isSymbolicLink());
console.log(JSON.stringify(await promises.readdir('async-dir')));
await promises.access('async.txt');
await promises.rename('async.txt','moved.txt');
console.log(await new Promise(resolve=>fs.exists('moved.txt',resolve)));
for(const operation of [()=>promises.readFile('missing'),()=>promises.access('missing'),()=>promises.rename('missing','dest')]) {
    try{await operation();}catch(error){console.log(error.code,error.syscall,error.path,error.dest);}
}
let synchronous=true;
const invalid=promises.readFile(null).catch(error=>console.log('invalid',synchronous,error.name,error.code));
synchronous=false;await invalid;
await promises.unlink('moved.txt');await promises.rmdir('async-dir/child');await promises.rmdir('async-dir');
try { await promises.readFile(1); } catch(e) { console.log(e.name,e.code); }
console.log('done');
