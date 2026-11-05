import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
let observed;
const server=createServer(async(req,res)=>{
 const chunks=[];for await(const bytes of req)chunks.push(bytes);
 observed=[req.method,req.url,req.complete,Buffer.concat(chunks).toString(),req.rawTrailers,req.trailers,req.headers.cookie,req.headers['x-repeat'],req.headersDistinct['x-repeat']];
 res.sendDate=false;res.writeHead(201,{'Content-Type':'text/plain'});res.write('');res.write('one');res.write('');res.addTrailers({'X-End':'done'});res.end('two');
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');
socket.write('POST /upload HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\nConnection: close\r\nCookie: a=1\r\nCookie: b=2\r\nX-Repeat: a\r\nX-Repeat: b\r\n\r\n');
for(const part of ['4\r\nw','iki\r\n','5\r\npedia\r\n','0\r\nX-Trailer: yes\r\n\r\n']){socket.write(part);await new Promise(resolve=>setTimeout(resolve,2));}
let text='';for await(const chunk of socket)text+=chunk.toString();
console.log(JSON.stringify(observed));console.log(JSON.stringify(text));
await new Promise(resolve=>server.close(resolve));
