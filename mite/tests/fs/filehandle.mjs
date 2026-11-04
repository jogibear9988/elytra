import fs from 'node:fs';
import promises, {open} from 'node:fs/promises';
import {EventEmitter} from 'node:events';
const h=await open('handle.bin','w+',0o600);
console.log(h.constructor.name,h instanceof EventEmitter,Number.isInteger(h.fd),h.fd>=0);
const data=Buffer.from('abcé');
const w=await h.write(data);console.log(w.bytesWritten,w.buffer===data,Object.getPrototypeOf(w)===null);
const target=Buffer.alloc(9,0x2e);
const r=await h.read(target,{offset:1,length:5,position:0});
console.log(r.bytesRead,r.buffer===target,Object.getPrototypeOf(r)===null,target.toString('hex'));
console.log((await h.stat()).size,(await h.stat({bigint:true})).size===5n);
console.log(await h.sync(),await h.datasync());
await h.truncate(3);
await h.writeFile('tail');
console.log(fs.readFileSync('handle.bin').toString('hex'));
await promises.appendFile(h,'!');
console.log((await promises.readFile(h)).length);
const readHandle=await open('handle.bin');
console.log((await readHandle.read({length:3,position:0})).bytesRead);
console.log((await readHandle.readFile('hex')));
await readHandle[Symbol.asyncDispose]();
let event=0;h.once('close',()=>{++event;console.log('close event',h.fd);});
const pending=h.close();console.log('closed fd',h.fd);await pending;await h.close();
console.log(event,h.fd);
for(const [method,args] of [['read',[]],['write',['x']],['stat',[]],['truncate',[]],['sync',[]],['datasync',[]],['readFile',[]],['writeFile',['x']],['appendFile',['x']]]) {
    try{await h[method](...args);}catch(e){console.log(method,e.name,e.code,e.syscall);}
}
