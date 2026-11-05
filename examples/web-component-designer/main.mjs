import {startServer} from './server.mjs';
import {randomUUID} from 'node:crypto';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';

const web=fileURLToPath(new URL('./web/',import.meta.url));
let report;
const smoke=process.argv.includes('--smoke')?{
  token:randomUUID(),
  script:await readFile(fileURLToPath(new URL('./smoke.js',import.meta.url))),
  report:body=>report(body)
}:undefined;
const server=await startServer(web,smoke);
const url=`http://127.0.0.1:${server.address().port}/`;
if(process.argv.includes('--serve-only')){
  console.log(url);
}else{
  const {app,BrowserWindow}=await import('firefly');
  await app.whenReady();
  const window=new BrowserWindow({width:1280,height:850,title:'Web Component Designer · Firefly'});
  app.on('window-all-closed',()=>app.quit());
  app.on('quit',()=>server.close());
  if(smoke){
    const deadline=setTimeout(()=>{console.error('DESIGNER_SMOKE_TIMEOUT');app.exit(1);},45000);
    report=body=>{
      try{
        const result=JSON.parse(body);
        if(!result.ready||!result.edited||!result.monaco||!result.privilegesAbsent)throw Error(body);
        clearTimeout(deadline);console.log('DESIGNER_UI_EDIT_MONACO_ISOLATION_OK');window.destroy();
      }catch(error){console.error(error);app.exit(1);}
    };
  }
  try{await window.loadURL(url);console.log('DESIGNER_DOCUMENT_LOADED');}
  catch(error){console.error(error);app.exit(1);}
}
