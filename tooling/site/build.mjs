import {readFile,writeFile,mkdir,cp,readdir} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import MarkdownIt from 'markdown-it';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const out=path.resolve(root,process.argv[2]||'build/site');
if(out===root||!out.startsWith(root+path.sep)||out.startsWith(path.join(root,'vendor')+path.sep))throw Error('Site output must be a separate directory in this checkout');
const repo='https://github.com/jogibear9988/elytra';
const ref='master';
const escape=value=>value.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const urlPath=value=>value.split(path.sep).join('/');
const md=new MarkdownIt({html:false,linkify:false,typographer:false});
md.renderer.rules.table_open=()=>'<div class="table-scroll"><table>\n';
md.renderer.rules.table_close=()=>'</table></div>\n';
md.renderer.rules.heading_open=(tokens,index,options,env,renderer)=>{
  const raw=tokens[index+1].content.toLowerCase().replace(/[^\p{L}\p{N}\s-]/gu,'').trim().replace(/\s+/g,'-');
  const count=env.slugs.get(raw)||0;env.slugs.set(raw,count+1);
  tokens[index].attrSet('id',raw+(count?`-${count}`:''));
  return renderer.renderToken(tokens,index,options);
};
const linkRule=md.renderer.rules.link_open||((tokens,index,options,env,renderer)=>renderer.renderToken(tokens,index,options));
md.renderer.rules.link_open=(tokens,index,options,env,renderer)=>{
  const token=tokens[index],href=token.attrGet('href');
  if(href&&!/^(?:[a-z]+:|#|\/\/)/i.test(href)){
    const [local,anchor]=href.split('#',2),target=path.resolve(path.dirname(env.source),decodeURI(local));
    if(target.startsWith(root+path.sep)){
      if(env.markdown.has(target)){
        token.attrSet('href',urlPath(path.relative(path.dirname(env.source),target)).replace(/\.md$/,'.html')+(anchor?'#'+anchor:''));
      }else{
        token.attrSet('href',`${repo}/blob/${ref}/${urlPath(path.relative(root,target))}`+(anchor?'#'+anchor:''));
      }
    }
  }
  return linkRule(tokens,index,options,env,renderer);
};
async function collect(directory){let result=[];for(const item of await readdir(directory,{withFileTypes:true})){const name=path.join(directory,item.name);if(item.isDirectory())result.push(...await collect(name));else if(name.endsWith('.md'))result.push(name);}return result;}
const sources=[path.join(root,'README.md'),...await collect(path.join(root,'docs'))];
const markdown=new Set(sources);
await mkdir(out,{recursive:true});
for(const item of await readdir(path.join(root,'site')))await cp(path.join(root,'site',item),path.join(out,item),{recursive:true});
await writeFile(path.join(out,'.nojekyll'),'');
for(const source of sources){
  const relative=path.relative(root,source),destination=path.join(out,relative.replace(/\.md$/,'.html'));
  const prefix=urlPath(path.relative(path.dirname(destination),out))||'.';
  const sourceText=await readFile(source,'utf8');
  const title=sourceText.match(/^# (.+)$/m)?.[1]||path.basename(source,'.md');
  const content=md.render(sourceText,{source,markdown,slugs:new Map()});
  const links=[['docs/getting-started.html','Getting started'],['docs/api/index.html','API overview'],['docs/api/mite.html','Mite runtime'],['docs/api/firefly.html','Firefly desktop'],['docs/compatibility/mite-0.1.html','Mite compatibility'],['docs/compatibility/firefly-0.1.html','Firefly compatibility'],['docs/PROGRESS.html','Progress'],['docs/development/renderer-build.html','Build Firefly']];
  const nav=links.map(([href,label])=>`<a href="${prefix}/${href}"${relative.replace(/\.md$/,'.html')===href?' aria-current="page"':''}>${label}</a>`).join('');
  const html=`<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="theme-color" content="#f8f5eb"><title>${escape(title)} — Mite + Firefly</title><link rel="icon" href="${prefix}/mark.svg" type="image/svg+xml"><link rel="stylesheet" href="${prefix}/styles.css"></head><body><a class="skip" href="#main">Skip to content</a><header class="nav wrap"><a class="brand" href="${prefix}/"><img src="${prefix}/mark.svg" width="30" height="30" alt="">elytra</a><nav aria-label="Main"><a class="hide-small" href="${prefix}/#projects">Projects</a><a href="${prefix}/docs/api/index.html">Docs</a><a class="github-link" href="${repo}">GitHub ↗</a></nav></header><div class="wrap docs-layout"><aside class="docs-side"><nav aria-label="Documentation"><div class="eyebrow">DOCUMENTATION</div>${nav}</nav></aside><main class="doc-article" id="main">${content}<a class="doc-source" href="${repo}/blob/${ref}/${urlPath(relative)}">View this page on GitHub ↗</a></main></div><footer class="wrap footer"><a class="brand" href="${prefix}/">elytra<span>/</span></a><p>Mite + Firefly · Open source, under development.</p><a href="${repo}">Contribute ↗</a></footer></body></html>`;
  await mkdir(path.dirname(destination),{recursive:true});await writeFile(destination,html);
}
console.log(`Built homepage and ${sources.length} documentation pages in ${path.relative(root,out)}`);
