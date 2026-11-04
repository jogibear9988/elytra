const {Socket,Server}=require('node:net');
const calls=[
 ()=>new Socket({fd:1}),
 ()=>new Socket({blockList:{}}),
 ()=>new Socket().connect('/tmp/mite-unix-socket'),
 ()=>new Socket().connect({port:80,host:'127.0.0.1',localAddress:'127.0.0.1'}),
 ()=>new Socket().connect({port:80,host:'127.0.0.1',signal:{}}),
 ()=>new Server({blockList:{}}),
 ()=>new Server().listen({port:0,reusePort:true}),
 ()=>new Socket().resetAndDestroy()
];
for(const call of calls){try{call();console.log('unexpected success');}catch(e){console.log(e.code);}}
