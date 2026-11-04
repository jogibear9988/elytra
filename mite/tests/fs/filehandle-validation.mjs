import fs from 'node:fs';
const h=await fs.promises.open('validate-handle','w+');
await h.writeFile('abcdef');
const cases=[['read',[Buffer.alloc(2),0,3,0]],['read',[{length:-1}]],['read',[{position:'1'}]],
             ['write',[Buffer.alloc(2),-1]],['write',['a',0,'invalid']],['truncate',[-1]],['truncate',[1.5]],
             ['stat',[null]],['read',[null]],['read',[Buffer.alloc(0)]],['write',[Buffer.alloc(0)]],
             ['writeFile',[{}]],['write',[null]]];
for(const [method,args] of cases) {
    try{const v=await h[method](...args);console.log(method,v?.bytesRead??v?.bytesWritten??v?.size??String(v));}
    catch(e){console.log(method,e.name,e.code);}
}
await h.close();
