import {createServer,connect} from 'node:net';
import {once} from 'node:events';
const server=createServer(socket=>socket.end('IPv6'));
server.listen({host:'::1',port:0,ipv6Only:true});await once(server,'listening');
console.log(server.address().address,server.address().family);
const client=connect({port:server.address().port,host:'::1'});
let text='';for await(const chunk of client)text+=chunk;
console.log(text);await new Promise(resolve=>server.close(resolve));
