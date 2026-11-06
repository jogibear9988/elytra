/** Abort between listen() returning and startup must suppress the listening event. */
import { createServer } from 'node:net';
const controller = new AbortController();
const server = createServer();
let listening = 0;

server.on('listening', () => listening++);

server.once('close', () => console.log('immediate server', listening, server.listening));

server.listen({ port: 0, host: '127.0.0.1', signal: controller.signal });
controller.abort();
