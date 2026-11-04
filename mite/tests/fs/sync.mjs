import fs, {readFileSync, writeFileSync} from 'node:fs';
function error(fn) { try { fn(); return 'no error'; } catch(e) { return [e.name,e.code,e.errno<0,e.syscall,e.path,e.dest]; } }
writeFileSync('text.txt','é文😀\n');
fs.appendFileSync('text.txt','tail','utf8');
console.log(readFileSync('text.txt','utf8'));
const view = new Uint8Array([9,0,128,255,7]).subarray(1,4);
fs.writeFileSync('bytes.bin',view);
console.log(Buffer.isBuffer(fs.readFileSync('bytes.bin')), fs.readFileSync(Buffer.from('bytes.bin')).toString('hex'));
console.log(JSON.stringify(error(()=>fs.writeFileSync('text.txt','bad',{flag:'wx'}))));
console.log(fs.mkdirSync('nested/child',{recursive:true}), fs.mkdirSync('nested/child',{recursive:true}));
console.log(JSON.stringify(error(()=>fs.readFileSync('nested'))));
console.log(JSON.stringify(error(()=>fs.readFileSync('missing'))));
console.log(JSON.stringify(error(()=>fs.renameSync('missing','target'))));
const st=fs.statSync('bytes.bin');
console.log(st instanceof fs.Stats,st.size,st.isFile(),st.isDirectory(),st.atime instanceof Date,Number.isFinite(st.mtimeMs));
const big=fs.statSync('bytes.bin',{bigint:true});
console.log(typeof big.size,big instanceof fs.Stats,big.size===3n,typeof big.mtimeNs,big.mtimeNs/1000000n===big.mtimeMs,big.isFile());
console.log(fs.lstatSync('link').isSymbolicLink(), fs.statSync('link').isFile());
console.log(fs.statSync('missing',{throwIfNoEntry:false}), fs.existsSync('missing'),fs.existsSync('bytes.bin'));
console.log(JSON.stringify(fs.readdirSync('.').sort()));
console.log(JSON.stringify(fs.readdirSync('.',{withFileTypes:true}).map(e=>[e.name,e.isFile(),e.isDirectory(),e.isSymbolicLink(),e.parentPath,Object.keys(e).sort().join(",")]).sort()));
console.log(fs.readdirSync('.',{encoding:'buffer'}).every(Buffer.isBuffer));
fs.writeFileSync('rename-me','x'); fs.renameSync('rename-me','renamed'); fs.unlinkSync('renamed');
fs.rmdirSync('nested/child');fs.rmdirSync('nested');
console.log(fs.existsSync('renamed'),fs.existsSync('nested'));
const raw=Buffer.from([114,97,119,45,255]);
if(process.platform!=='win32') {
    try {
        fs.writeFileSync(raw,'raw data');
        console.log(fs.readFileSync(raw,'utf8'),fs.readdirSync('.',{encoding:'buffer'}).some(name=>name.equals(raw)));
        fs.unlinkSync(raw);
    } catch(e) { console.log(e.name,e.code,e.syscall); }
}
