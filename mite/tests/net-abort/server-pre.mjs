/** A pre-aborted listen must close without binding or emitting listening. */
import { createServer } from 'node:net';
const controller = new AbortController();
controller.abort('before listen');
const server = createServer();
let listening = 0;
let errors = 0;
let returned = false;

server.on('listening', () => listening++);

server.on('error', () => errors++);

server.once('close', () => {
  console.log(
    'server pre',
    returned,
    listening,
    errors,
    server.listening,
    server.address() === null
  );
});

server.listen({ port: 0, host: '127.0.0.1', signal: controller.signal });
returned = true;
