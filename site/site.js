const tabs=[...document.querySelectorAll('[role="tab"]')];
function selectTab(tab){
  for(const item of tabs){const active=item===tab;item.setAttribute('aria-selected',String(active));item.tabIndex=active?0:-1;document.getElementById(item.getAttribute('aria-controls')).hidden=!active;}
}
for(const tab of tabs){
  tab.addEventListener('click',()=>selectTab(tab));
  tab.addEventListener('keydown',event=>{let index=tabs.indexOf(tab);if(event.key==='ArrowRight')index=(index+1)%tabs.length;else if(event.key==='ArrowLeft')index=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')index=0;else if(event.key==='End')index=tabs.length-1;else return;event.preventDefault();selectTab(tabs[index]);tabs[index].focus();});
}
for(const button of document.querySelectorAll('[data-copy]'))button.addEventListener('click',async()=>{
  const code=document.getElementById(button.dataset.copy);
  try{await navigator.clipboard.writeText(code.textContent);document.getElementById('copy-status').textContent='Code copied to clipboard.';button.textContent='Copied ✓';}
  catch{const range=document.createRange();range.selectNodeContents(code);const selection=window.getSelection();selection.removeAllRanges();selection.addRange(range);document.getElementById('copy-status').textContent='Code selected. Use your system copy command.';button.textContent='Code selected';}
});
