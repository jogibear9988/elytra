import {createServer,connect} from 'node:net';
import {once} from 'node:events';
const server=createServer({allowHalfOpen:true},socket=>{
 // No data listener until later: starting native reads must retain stream data.
 setTimeout(async()=>{let text='';for await(const bytes of socket)text+=bytes;socket.end(text.toUpperCase());},10);
});
server.listen(0,'127.0.0.1');await once(server,'listening');
const socket=connect(server.address().port,'127.0.0.1');socket.end('buffer me');
let result='';for await(const bytes of socket)result+=bytes;
console.log(result);await new Promise(resolve=>server.close(resolve));
