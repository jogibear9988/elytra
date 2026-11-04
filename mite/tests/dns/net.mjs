import {createServer,connect} from 'node:net';
import {once} from 'node:events';
for(const family of [4,6,undefined]) {
 const host=family===6?'::1':'127.0.0.1';
 const server=createServer(socket=>socket.end('resolved'));
 server.listen(0,host);await once(server,'listening');
 const client=connect({port:server.address().port,...(family?{host:'localhost',family}:{})});
 client.on('lookup',(err,address,kind,hostname)=>{console.log('lookup',err===null,typeof address,kind===4||kind===6,hostname);});
 let text='';for await(const bytes of client)text+=bytes;
 console.log('connected',family??'auto',text,client.autoSelectFamilyAttemptedAddresses?.length??0);
 await new Promise(resolve=>server.close(resolve));
}
const server=createServer(socket=>socket.end('named server'));
server.listen(0,'localhost');await once(server,'listening');
const client=connect({port:server.address().port,host:'localhost',family:server.address().family==='IPv6'?6:4});
let text='';for await(const bytes of client)text+=bytes;console.log(text);
await new Promise(resolve=>server.close(resolve));
