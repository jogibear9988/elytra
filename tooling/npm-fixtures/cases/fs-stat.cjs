const stats=require('@nodelib/fs.stat');
const fs=require('node:fs');const path=require('node:path');
const root=process.argv[2];
function inspect(name,options){
    try{const s=stats.statSync(path.join(root,name),options);return [s.isFile(),s.isDirectory(),s.isSymbolicLink(),typeof s.size];}
    catch(e){return [e.code,e.syscall];}
}
console.log(JSON.stringify(['plain.txt','folder','link','dir-link','broken','missing'].map(name=>inspect(name))));
console.log(JSON.stringify(inspect('link',{followSymbolicLink:false})));
console.log(JSON.stringify(inspect('link',new stats.Settings({markSymbolicLink:true}))));
console.log(JSON.stringify(inspect('broken',{throwErrorOnBrokenSymbolicLink:false})));
const calls=[];
const settings=new stats.Settings({fs:{lstatSync(path){calls.push('lstat');return fs.lstatSync(path);}}});
console.log(stats.statSync(path.join(root,'plain.txt'),settings).size,calls.join(','));
console.log(require('@nodelib/fs.stat/package.json').version,stats===require('@nodelib/fs.stat'));
