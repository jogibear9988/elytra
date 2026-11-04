import net,{Socket,Server} from 'node:net';
import {once} from 'node:events';
const server=new Server({allowHalfOpen:true},socket=>{
 let text='';socket.setEncoding('utf8');socket.on('data',chunk=>text+=chunk);
 socket.on('end',()=>{
  console.log('half',text,socket.readableEnded,socket.writableEnded);
  setTimeout(()=>socket.end('reply'),5);
 });
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const client=new Socket();client.connect({port:server.address().port,host:'127.0.0.1'});
client.end('request');
let result='';for await(const chunk of client)result+=chunk;
console.log('reply',result);
await new Promise(resolve=>server.close(resolve));
console.log('identity',net.Socket===Socket,net.Server===Server,net.connect===net.createConnection);
