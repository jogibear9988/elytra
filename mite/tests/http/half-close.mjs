import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
const server=createServer((req,res)=>{res.sendDate=false;setTimeout(()=>res.end(req.url),5);});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');
socket.end('GET /one HTTP/1.1\r\nHost: test\r\n\r\nGET /two HTTP/1.1\r\nHost: test\r\n\r\n');
let text='';for await(const bytes of socket)text+=bytes.toString();console.log(JSON.stringify(text));
await new Promise(resolve=>server.close(resolve));
