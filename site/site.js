const tabs=[...document.querySelectorAll('[role="tab"]')];
function selectTab(tab){
  for(const item of tabs){const active=item===tab;item.setAttribute('aria-selected',String(active));item.tabIndex=active?0:-1;document.getElementById(item.getAttribute('aria-controls')).hidden=!active;}
}
for(const tab of tabs){
  tab.addEventListener('click',()=>selectTab(tab));
  tab.addEventListener('keydown',event=>{let index=tabs.indexOf(tab);if(event.key==='ArrowRight')index=(index+1)%tabs.length;else if(event.key==='ArrowLeft')index=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')index=0;else if(event.key==='End')index=tabs.length-1;else return;event.preventDefault();selectTab(tabs[index]);tabs[index].focus();});
}
for(const button of document.querySelectorAll('[data-copy]'))button.addEventListener('click',async()=>{try{await navigator.clipboard.writeText(document.getElementById(button.dataset.copy).textContent);button.textContent='Copied';}catch{button.textContent='Select and copy the code';}});
