import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
const size=512*1024+11;let received;
const server=createServer(async(req,res)=>{
 res.sendDate=false;
 if(req.url==='/upload'){
  await new Promise(resolve=>setTimeout(resolve,5));
  let count=0,sum=0;for await(const bytes of req){count+=bytes.length;for(const b of bytes)sum=(sum+b)>>>0;}
  received=[count,sum,req.complete];res.end('uploaded');
 }else res.end('next');
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');
const payload=Buffer.alloc(size);for(let i=0;i<size;i++)payload[i]=i%251;
socket.write(`POST /upload HTTP/1.1\r\nHost: test\r\nContent-Length: ${size}\r\n\r\n`);socket.write(payload);
let text='',sent=false;for await(const bytes of socket){text+=bytes.toString();if(!sent&&text.endsWith('uploaded')){sent=true;socket.write('GET /next HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n');}}
console.log(JSON.stringify(received));console.log(JSON.stringify(text));await new Promise(resolve=>server.close(resolve));
