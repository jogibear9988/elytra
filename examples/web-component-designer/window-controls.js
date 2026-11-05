// Sample-local capability for this one window, not a general Firefly IPC API.
(() => {
  const token=document.querySelector('meta[name="firefly-window-token"]')?.content;
  if(!token)return;
  async function command(action){
    const response=await fetch('/__firefly_window/'+action,{method:'POST',headers:{'X-Firefly-Window-Token':token}});
    if(!response.ok)throw Error('Window action failed: '+response.status);
    const state=await response.json();
    const maximize=document.getElementById('window-maximize');
    maximize.setAttribute('aria-label',state.maximized?'Restore window':'Maximize window');
    maximize.title=state.maximized?'Restore window':'Maximize window';
    maximize.textContent=state.maximized?'❐':'□';
    return state;
  }
  // A tiny API on the page only exposes app-owned, fixed window operations.
  // There is no channel name, executable path, script source or arbitrary payload.
  const controls=Object.freeze({minimize:()=>command('minimize'),maximize:()=>command('maximize'),
    unmaximize:()=>command('unmaximize'),restore:()=>command('restore'),state:()=>command('state')});
  Object.defineProperty(window,'designerWindow',{value:controls});
  document.getElementById('window-minimize').addEventListener('click',()=>controls.minimize().catch(console.error));
  document.getElementById('window-maximize').addEventListener('click',async()=>{
    try{const state=await controls.state();await (state.maximized?controls.unmaximize():controls.maximize());}catch(error){console.error(error);}
  });
  document.getElementById('window-close').addEventListener('click',()=>command('close').catch(console.error));
  command('state').catch(console.error);
})();
