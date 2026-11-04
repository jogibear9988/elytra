const net=require('node:net');
const server=net.createServer();
console.log(server.unref()===server,server.ref()===server);
server.listen(0,'127.0.0.1',()=>{
 console.log('unreferenced',server.unref()===server);
});
