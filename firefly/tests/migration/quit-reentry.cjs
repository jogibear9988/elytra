const {app,BrowserWindow}=require(process.env.FIREFLY_API_MODULE || 'firefly');
const trace=[];
let repeated=false;
app.on('before-quit',()=>{
 trace.push('before');
 if(!repeated){repeated=true;app.quit();}
});
app.on('will-quit',()=>{trace.push('will');app.quit();});
app.on('quit',(_event,code)=>{trace.push(`quit:${code}`);console.log(JSON.stringify({trace}));});
app.whenReady().then(()=>{
 const win=new BrowserWindow({show:false});
 win.on('close',()=>trace.push('close'));
 win.on('closed',()=>trace.push('closed'));
 app.quit();
});
