/** A stale Unix socket remains foreign filesystem state, never silently unlinked. */
import net from 'node:net';
import fs from 'node:fs';
import { once } from 'node:events';
const endpoint = process.env.MITE_STALE_PIPE;
if (process.platform === 'win32' || !endpoint || !fs.lstatSync(endpoint).isSocket()) {
  throw Error('Require real Python-created stale Unix socket');
}
const client = net.connect({ path: endpoint });
const closed = new Promise((resolve) => client.on('close', resolve));
const [failure] = await once(client, 'error');
await closed;
if (
  failure.code !== 'ECONNREFUSED' ||
  failure.syscall !== 'connect' ||
  !fs.lstatSync(endpoint).isSocket()
) {
  throw Error('Stale Unix connect error or path ownership');
}
const server = net.createServer();
server.listen(endpoint);
const [bind] = await once(server, 'error');
if (bind.code !== 'EADDRINUSE' || !fs.lstatSync(endpoint).isSocket()) {
  throw Error('Bind removed stale foreign endpoint');
}
console.log('stale Unix socket preserved', failure.code, bind.code);
