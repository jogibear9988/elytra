import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
const order=[];
const server=createServer((req,res)=>{
 order.push(req.url);res.sendDate=false;
 if(req.url==='/slow')setTimeout(()=>res.end('first'),10);
 else res.end('second');
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');
socket.write('GET /slow HTTP/1.1\r\nHost: test\r\n\r\nGET /fast HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n');
let text='';for await(const chunk of socket)text+=chunk.toString();
console.log(JSON.stringify(order));console.log(JSON.stringify(text));
await new Promise(resolve=>server.close(resolve));
