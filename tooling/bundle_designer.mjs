// Build tool only. esbuild comes from the reviewed, pinned demo lockfile.
import {readFile,writeFile,readdir,mkdir,rm,copyFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import path from 'node:path';
import vm from 'node:vm';
const [source,web]=process.argv.slice(2).map(value=>path.resolve(value));
if(!source||!web)throw Error('Usage: bundle_designer.mjs source web');
const {build,transform}=createRequire(path.join(source,'package.json'))('esbuild');
let html=await readFile(path.join(web,'index.html'),'utf8');
const match=/const importMap = (\{[\s\S]*?\n\s*\});/.exec(html);
if(!match)throw Error('Review pinned import-map extraction');
const imports=vm.runInNewContext('('+match[1]+')',Object.create(null),{timeout:1000}).imports;
// The pinned demo points DXF at a filename its pinned package no longer ships.
// Resolve that reviewed browser entry from the actual package's module field.
const dxf=JSON.parse(await readFile(path.join(web,'node_modules/@tarikjabiri/dxf/package.json'),'utf8'));
imports['@tarikjabiri/dxf']='./node_modules/@tarikjabiri/dxf/'+dxf.module.replace(/^\.\//,'');
const urlFor=file=>'/'+path.relative(web,file).split(path.sep).join('/');
const entries={'app-shell':path.join(web,'dist/appShell.js')};
const entryKeys={};let number=0;
for(const [key,value] of Object.entries(imports)){
 if(key.endsWith('/')||key.startsWith('max_'))continue;
 const name='package-'+number++;entries[name]=path.resolve(web,value);entryKeys[key]=name;
}
const prefix=Object.keys(imports).filter(key=>key.endsWith('/')).sort((a,b)=>b.length-a.length);
function resolveImport(specifier,importer){
 if(imports[specifier])return path.resolve(web,imports[specifier]);
 const base=prefix.find(key=>specifier.startsWith(key));
 if(base)return path.resolve(web,imports[base]+specifier.slice(base.length));
 if(specifier.startsWith('.'))return path.resolve(path.dirname(importer),specifier);
 return null;
}
const plugin={name:'designer-browser-layout',setup(api){
 api.onResolve({filter:/.*/},args=>{
  if(args.kind==='entry-point')return;
  const target=resolveImport(args.path,args.importer);
  if(target&&(imports[args.path]||args.with.type==='css'||prefix.some(key=>args.path.startsWith(key))))return {path:target,namespace:args.with.type==='css'?'sheet':'file'};
 });
 api.onLoad({filter:/.*/,namespace:'sheet'},async args=>{
  const css=(await readFile(args.path,'utf8')).replace(/url\(\s*(['"]?)([^)'"\s]+)\1\s*\)/g,(all,quote,url)=>/^(?:[a-z]+:|\/|#)/i.test(url)?all:`url(${JSON.stringify(path.posix.join(path.posix.dirname(urlFor(args.path)),url))})`);
  return {contents:`const sheet=new CSSStyleSheet();sheet.replaceSync(${JSON.stringify(css)});export default sheet;`,loader:'js',resolveDir:path.dirname(args.path)};
 });
 api.onLoad({filter:/\.(?:m?js)$/},async args=>{
  const code=await readFile(args.path,'utf8');
  // Preserve asset URL bases after moving modules into shared chunks.
  const transformed=code.includes('import.meta.url')?await transform(code,{loader:'js',define:{'import.meta.url':'__fireflyOriginalModuleUrl'},banner:`const __fireflyOriginalModuleUrl=new URL(${JSON.stringify(urlFor(args.path))},location.origin).href;`}):{code};
  return {contents:transformed.code,loader:'js',resolveDir:path.dirname(args.path)};
 });
}};
const classic=new Set([...html.replace(/<!--[\s\S]*?-->/g,'').matchAll(/<script[^>]+src=["']([^"']+)["']/g)].map(match=>path.resolve(web,match[1])).filter(file=>file.startsWith(path.join(web,'node_modules')+path.sep)));
for(const file of classic){const code=await readFile(file,'utf8');const result=await transform(code,{minify:true,legalComments:'inline'});await writeFile(file,result.code);}
const outdir=path.join(web,'bundle');await mkdir(outdir,{recursive:true});
const result=await build({entryPoints:entries,outdir,bundle:true,splitting:true,format:'esm',platform:'browser',
 target:'es2022',minify:true,metafile:true,legalComments:'external',plugins:[plugin],logLevel:'warning',
 loader:{'.woff':'file','.woff2':'file','.ttf':'file','.svg':'file','.png':'file'}});
const mapped={};for(const [key,name] of Object.entries(entryKeys))mapped[key]='./bundle/'+name+'.js';
// Dynamic imports from the app's existing importShim calls retain their names.
html=html.replace(match[0],'const importMap = '+JSON.stringify({imports:mapped},null,2)+';');
html=html.replace("importShim('./dist/appShell.js')","importShim('./bundle/app-shell.js')");
await writeFile(path.join(web,'index.html'),html);
// Keep upstream's explicitly built workers as standalone browser entry points,
// minified in place. They are independently bundled by the pinned Monaco script.
const editor=path.join(web,'node_modules/monaco-editor/esm/vs/editor');
const workers=(await readdir(editor)).filter(name=>name.endsWith('.worker.js'));
for(const name of workers)await build({entryPoints:[path.join(editor,name)],outfile:path.join(editor,name),bundle:true,
 allowOverwrite:true,minify:true,format:'esm',platform:'browser',legalComments:'external'});
const notices=path.join(web,'../licenses/npm');await mkdir(notices,{recursive:true});
let removedBytes=0,keptBytes=0;
async function compact(directory){
 for(const entry of await readdir(directory,{withFileTypes:true})){
  const file=path.join(directory,entry.name);
  if(entry.isDirectory()){await compact(file);continue;}
  const bytes=await readFile(file),relative=path.relative(path.join(web,'node_modules'),file);
  const notice=/^(?:licen[sc]e|copying|notice|copyright)(?:[. _-]|$)/i.test(entry.name)||entry.name==='package.json';
  if(notice){const dest=path.join(notices,relative);await mkdir(path.dirname(dest),{recursive:true});await copyFile(file,dest);}
  const binary=bytes.length>4&&['cffaedfe','feedfacf','7f454c46','4d5a9000'].includes(bytes.subarray(0,4).toString('hex'));
  const worker=file.startsWith(editor+path.sep)&&(entry.name.endsWith('.worker.js')||entry.name.endsWith('.worker.js.LEGAL.txt'));
  const remove=!worker&&!classic.has(file)&&(/\.(?:[cm]?js|map|ts|node|exe|dll|dylib|so|a)$/i.test(entry.name)||binary||(notice&&entry.name!=='package.json'));
  if(remove){removedBytes+=bytes.length;await rm(file);}else keptBytes+=bytes.length;
 }
}
await compact(path.join(web,'node_modules'));
// The source module graph is represented by app-shell.js plus its shared chunks.
async function pruneSourceModules(directory){
 for(const entry of await readdir(directory,{withFileTypes:true})){
  const file=path.join(directory,entry.name);
  if(entry.isDirectory())await pruneSourceModules(file);
  else if(/\.(?:[cm]?js|map|ts)$/.test(entry.name))await rm(file);
 }
}
await pruneSourceModules(path.join(web,'dist'));
await writeFile(path.join(web,'bundle/build-meta.json'),JSON.stringify(result.metafile,null,2));
await writeFile(path.join(web,'bundle/BUILD.json'),JSON.stringify({bundler:'esbuild',entries:Object.keys(entries).length,
 workers,removedModuleBytes:removedBytes,retainedAssetBytes:keptBytes,sourceMap:false},null,2)+'\n');
console.log(`Bundled ${Object.keys(entries).length} app/package entry points and ${workers.length} workers; removed ${(removedBytes/1024**2).toFixed(1)} MiB of module/build payload.`);
