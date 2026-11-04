import {createServer,connect} from 'node:net';
import {once} from 'node:events';
const server=createServer();server.listen(0,'127.0.0.1');await once(server,'listening');
const port=server.address().port;await new Promise(resolve=>server.close(resolve));
const client=connect(port,'127.0.0.1');
client.on('error',e=>console.log('error',e.code,e.syscall,e.address,e.port===port,typeof e.errno));
client.on('close',had=>console.log('close',had,client.connecting,client.destroyed));
