import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
const server=createServer(async(req,res)=>{let text='';for await(const bytes of req)text+=bytes.toString();res.sendDate=false;res.end(text.toUpperCase());});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');
socket.write('POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 5\r\nExpect: 100-continue\r\nConnection: close\r\n\r\n');
let result='',sent=false;
for await(const chunk of socket){result+=chunk.toString();if(!sent&&result.includes('100 Continue\r\n\r\n')){sent=true;socket.write('hello');}}
console.log(JSON.stringify(result));await new Promise(resolve=>server.close(resolve));
