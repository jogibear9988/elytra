/** An unhandled request certificate failure must remain fatal and suppress later JS. */
import { get } from 'node:https';
import { readFileSync } from 'node:fs';
const [port, directory] = process.argv.slice(2);
const pending = get({
  host: '127.0.0.1',
  port: Number(port),
  servername: 'wrong.example',
  ca: readFileSync(directory + '/ca.pem'),
  agent: false
});

pending.on('close', () => console.log('UNREACHABLE after fatal error'));
