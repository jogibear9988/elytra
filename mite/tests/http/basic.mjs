import http,{createServer,IncomingMessage,ServerResponse,OutgoingMessage} from 'node:http';
import {Server as TCPServer,connect} from 'node:net';
import {Readable} from 'node:stream';
import {once} from 'node:events';
let observed;
const server=createServer((req,res)=>{
 observed=[req instanceof IncomingMessage,req instanceof Readable,res instanceof ServerResponse,res instanceof OutgoingMessage,req.method,req.url,req.httpVersion,req.complete,req.headers.host];
 res.sendDate=false;res.setHeader('X-Result','yes');res.end('Grüße 🪲');
});
console.log(http.createServer===createServer,server instanceof TCPServer);
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');socket.write('GET /hello?q=1 HTTP/1.1\r\nHost: example.test\r\nConnection: close\r\n\r\n');
let text='';for await(const chunk of socket)text+=chunk.toString();
console.log(JSON.stringify(observed));console.log(JSON.stringify(text));
await new Promise(resolve=>server.close(resolve));
