/** Connect to a TLS echo peer using default trust or an explicitly supplied test CA. */
import { connect } from 'node:tls';
import { readFileSync } from 'node:fs';
import { isIP } from 'node:net';

const [host = 'localhost', port = '8443', caFile, servername] = process.argv.slice(2);
const socket = connect(
  {
    host,
    port: Number(port),
    servername: servername ?? (isIP(host) ? undefined : host),
    ...(caFile ? { ca: readFileSync(caFile) } : {}),
    signal: AbortSignal.timeout(5000)
  },
  () => {
    console.log('Secure:', socket.authorized, socket.getProtocol());
    socket.end('Hello through TLS');
  }
);

socket.on('data', (data) => console.log(data.toString()));

socket.on('error', (error) => {
  console.error(error.code, error.message);
  process.exitCode = 1;
});
