/** Oversize native Unix addresses fail, never bind/connect a truncated name. */
import net from 'node:net';
import fs from 'node:fs';
import path from 'node:path';
import { once } from 'node:events';
if (process.platform === 'win32') {
  throw Error('Unix sockaddr_un test only');
}
const endpoint = process.env.MITE_TEST_PIPE + 'x'.repeat(200);
const directory = path.dirname(endpoint);
const before = fs.readdirSync(directory).sort().join('\n');
const server = net.createServer();
server.listen({ path: endpoint });
const [bind] = await once(server, 'error');
if (bind.code !== 'EINVAL' || bind.syscall !== 'listen') {
  throw Error('Oversize Unix bind must fail with native EINVAL');
}
const socket = net.connect(endpoint);
const closed = new Promise((resolve) => socket.on('close', resolve));
const [connect] = await once(socket, 'error');
await closed;
if (connect.code !== 'EINVAL' || connect.syscall !== 'connect') {
  throw Error('Oversize Unix connect must fail with native EINVAL');
}
if (fs.readdirSync(directory).sort().join('\n') !== before) {
  throw Error('Oversize pipe path created a truncated filesystem endpoint');
}
console.log('oversize Unix endpoints refused without truncation', bind.code, connect.code);
