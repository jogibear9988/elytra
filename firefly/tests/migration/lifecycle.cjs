const {app,BrowserWindow}=require(process.env.FIREFLY_API_MODULE || 'firefly');
const fs=require('node:fs/promises');
const path=require('node:path');
const trace=[];
function check(value,message){if(!value)throw Error(message);}
app.on('ready',()=>trace.push('ready'));
app.on('before-quit',()=>trace.push('before-quit'));
app.on('will-quit',()=>trace.push('will-quit'));
app.on('quit',(_event,code)=>{trace.push(`quit:${code}`);console.log(JSON.stringify({trace}));});
app.on('window-all-closed',()=>{trace.push('window-all-closed');app.quit();});
check(!app.isReady(),'ready must be asynchronous');
let early=false;try{new BrowserWindow();}catch{early=true;}
check(early,'window before ready must fail');
app.whenReady().then(async()=>{
  check(app.isReady(),'ready state');
  const win=new BrowserWindow({width:640,height:400,title:'Migration fixture',show:true,
    webPreferences:{contextIsolation:true,nodeIntegration:false,sandbox:true}});
  const contents=win.webContents;
  check(BrowserWindow.fromId(win.id)===win && BrowserWindow.getAllWindows().length===1,'window registry');
  check(BrowserWindow.fromWebContents(win.webContents)===win,'webContents registry');
  let cancelled=false;
  win.on('close',event=>{
    if(!cancelled){cancelled=true;event.preventDefault();trace.push('close-cancelled');}
    else trace.push('close');
  });
  win.on('closed',()=>{check(win.isDestroyed()&&contents.isDestroyed(),'destroyed state');trace.push('closed');});
  win.webContents.once('did-finish-load',()=>trace.push('did-finish-load'));
  const isolated=new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('renderer isolation marker timed out')),10000);
    win.on('page-title-updated',(_event,title)=>{if(title==='RENDERER_ISOLATED'){clearTimeout(timer);resolve();}});
  });
  const page=path.join(__dirname,'page.html');
  const content=await fs.readFile(page,'utf8');check(content.includes('CAPABILITY_LEAK'),'Mite filesystem');
  await win.loadFile(page);trace.push('load-resolved');
  await isolated;check(win.webContents.getTitle()==='RENDERER_ISOLATED','renderer isolation');
  check(win.webContents.getOSProcessId()>0&&win.webContents.getOSProcessId()!==process.pid,'renderer OS process');
  check(win.webContents.getURL().startsWith('file:'),'loaded URL');
  win.setSize(800,500);check(JSON.stringify(win.getSize())==='[800,500]','native size');
  win.setTitle('Host title');check(win.getTitle()==='Host title','native title');
  win.hide();check(!win.isVisible(),'hide');win.show();check(win.isVisible(),'show');
  await new Promise(resolve=>setTimeout(resolve,100));
  win.close();check(!win.isDestroyed(),'cancelled close');win.close();
}).catch(error=>{console.error(error.stack);process.exitCode=1;for(const win of BrowserWindow.getAllWindows()){win.removeAllListeners('close');win.destroy();}app.quit();});
