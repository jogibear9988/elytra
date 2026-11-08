/** A genuine unreferenced native listener must not keep the process alive. */
import net from 'node:net';
const server = net.createServer();

server.listen({ path: process.env.MITE_TEST_PIPE }, () => {
  if (server.address() !== process.env.MITE_TEST_PIPE) {
    throw Error('Unref pipe listener address');
  }
  if (server.unref() !== server || server.ref() !== server || server.unref() !== server) {
    throw Error('Pipe ref/unref chaining');
  }
  console.log('unreferenced pipe exits naturally');
});
