const {app,BrowserWindow}=require(process.env.FIREFLY_API_MODULE || 'firefly');
const trace=[];
app.on('before-quit',event=>{trace.push('before');event.preventDefault();});
app.on('will-quit',event=>{trace.push('will');event.preventDefault();});
app.on('window-all-closed',()=>trace.push('window-all-closed'));
app.on('quit',(_event,code)=>{trace.push(`quit:${code}`);console.log(JSON.stringify({trace}));});
app.whenReady().then(()=>{
 process.exitCode=42;
 const win=new BrowserWindow({show:false});
 win.on('close',event=>{trace.push('close');event.preventDefault();});
 win.on('closed',()=>trace.push('closed'));
 app.exit(Number(process.env.FIREFLY_EXIT_CODE ?? '7'));
 console.log('EXIT_RETURNED');
 trace.push('after-exit');
});
