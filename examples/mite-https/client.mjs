/** Fetch an HTTPS response through verified native TLS, with a bounded application deadline. */
import { get } from 'node:https';
import { readFileSync } from 'node:fs';

const [address = 'https://localhost:8443/', caFile, servername] = process.argv.slice(2);
const request = get(
  address,
  {
    agent: false,
    ...(caFile ? { ca: readFileSync(caFile) } : {}),
    ...(servername ? { servername } : {}),
    signal: AbortSignal.timeout(5000)
  },
  (response) => {
    let body = '';
    response.setEncoding('utf8');

    response.on('data', (text) => (body += text));

    response.on('error', (error) => {
      console.error(error.code, error.message);
      process.exitCode = 1;
    });

    response.once('end', () => {
      console.log('Status:', response.statusCode);
      console.log(body);
    });
  }
);

request.on('error', (error) => {
  console.error(error.code, error.message);
  process.exitCode = 1;
});
