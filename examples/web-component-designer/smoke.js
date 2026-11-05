// Enabled only by the sample's explicit --smoke mode. This is an app-local test
// report, not a privileged IPC API; production mode has no report endpoint.
(async()=>{
  const token=new URL(document.currentScript.src).searchParams.get('token');
  async function report(result){await fetch('/__firefly_smoke_result?token='+encodeURIComponent(token),{method:'POST',body:JSON.stringify(result)});}
  try{
    await customElements.whenDefined('node-projects-app-shell');
    function find(root,selector){const match=root.querySelector(selector);if(match)return match;for(const element of root.querySelectorAll('*'))if(element.shadowRoot){const result=find(element.shadowRoot,selector);if(result)return result;}return null;}
    let container;
    for(let i=0;i<80&&!container;i++){container=find(document,'node-projects-document-container');if(!container)await new Promise(resolve=>setTimeout(resolve,250));}
    if(!container)throw Error('Designer document container did not appear');
    await container.whenReady();
    await container.setContentAsync('<button id="firefly-smoke-button">Firefly smoke</button>');
    await container.commitPendingChanges();
    if(!container.content.includes('firefly-smoke-button')||!container.content.includes('Firefly smoke'))throw Error('Document editing did not round-trip');
    container.currentView='code';
    await new Promise(resolve=>setTimeout(resolve,1000));
    if(!find(document,'.monaco-editor'))throw Error('Monaco editor is missing');
    if(typeof process!=='undefined'||typeof require!=='undefined')throw Error('Unexpected renderer privileges');
    await report({ready:true,edited:true,monaco:true,privilegesAbsent:true});
  }catch(error){await report({ready:false,error:String(error)});}
})();
