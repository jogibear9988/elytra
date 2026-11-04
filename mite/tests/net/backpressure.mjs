import {createServer,connect} from 'node:net';
import {once} from 'node:events';
const size=3*1024*1024+29;
let resolveReceived;
const received=new Promise(resolve=>resolveReceived=resolve);
const server=createServer({pauseOnConnect:true},socket=>{
 let count=0,sum=0;
 setTimeout(()=>{
  socket.on('data',bytes=>{count+=bytes.length;for(const b of bytes)sum=(sum+b)>>>0;});
  socket.on('end',()=>{socket.end(String(count));resolveReceived([count,sum]);});
  socket.resume();
 },10);
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect({port:server.address().port,host:'127.0.0.1',highWaterMark:1024});
await once(socket,'connect');
const payload=Buffer.alloc(size);for(let i=0;i<size;i++)payload[i]=i%251;
console.log('backpressure',socket.write(payload));
await once(socket,'drain');console.log('drained',socket.writableLength===0);
socket.end();let answer='';for await(const chunk of socket)answer+=chunk;
console.log('received',answer,...await received,socket.bytesWritten);
await new Promise(resolve=>server.close(resolve));
