const {Socket,Server}=require('node:net');
const calls=[
 ()=>new Socket({fd:1}),
 ()=>new Socket({blockList:{}}),
 ()=>new Socket().connect({port:80,host:'example.com'}),
 ()=>new Socket().connect('/tmp/mite-unix-socket'),
 ()=>new Socket().connect({port:80,host:'127.0.0.1',lookup(){}}),
 ()=>new Socket().connect({port:80,host:'127.0.0.1',autoSelectFamily:true}),
 ()=>new Server({blockList:{}}),
 ()=>new Server().listen({port:0,reusePort:true}),
 ()=>new Socket().resetAndDestroy()
];
for(const call of calls){try{call();console.log('unexpected success');}catch(e){console.log(e.code);}}
