/** Mite's explicit UTF-8 path and unsupported option policy; not a Node parity claim. */
import net from 'node:net';
import { once } from 'node:events';

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  if (caught?.code !== code) {
    throw Error('Expected ' + code + ', got ' + caught?.code);
  }
  return caught;
}

for (const path of ['', '/tmp/invalid\0path', '/tmp/invalid\ud800', '/tmp/invalid\udfff']) {
  fails(() => new net.Socket().connect({ path }), 'ERR_INVALID_ARG_VALUE');

  fails(() => net.createServer().listen({ path }), 'ERR_INVALID_ARG_VALUE');
}
for (const key of ['readableAll', 'writableAll']) {
  fails(
    () => net.createServer().listen({ path: process.env.MITE_TEST_PIPE, [key]: true }),
    'ERR_MITE_UNSUPPORTED_API'
  );
}
console.log(
  'PASS Mite strict malformed UTF-16 NUL empty paths and explicit unsupported permission options'
);

// Pipe initialization reserves native ownership before the asynchronous bind. Use
// one isolated endpoint and cancel every pending listen before any tick can bind.
const endpoint = process.env.MITE_TEST_PIPE;
if (!endpoint) {
  throw Error('Require isolated quota endpoint');
}
const pending = Array.from({ length: 4096 }, () => {
  const server = net.createServer();
  server.listen({ path: endpoint });
  return server;
});
const exhausted = fails(() => net.createServer().listen({ path: endpoint }), 'ENOSPC');
if (!Number.isInteger(exhausted.errno) || exhausted.errno >= 0) {
  throw Error('Native pipe initialization must preserve genuine negative libuv errno');
}
const closed = pending.map(
  (server) =>
    new Promise((resolve) => {
      server.once('close', resolve);
      server.close();
    })
);
const closing = fails(() => net.createServer().listen({ path: endpoint }), 'ENOSPC');
if (closing.errno !== exhausted.errno) {
  throw Error('Closing pipe handles must remain charged');
}
await Promise.all(closed);
const replacement = net.createServer();
replacement.listen({ path: endpoint });
await once(replacement, 'listening');
if (!replacement.listening || replacement.address() !== endpoint) {
  throw Error('Quota did not recover after actual native close callbacks');
}
await new Promise((resolve, reject) =>
  replacement.close((failure) => (failure ? reject(failure) : resolve()))
);
console.log(
  'PASS Mite 4096 pending and closing pipe ownership quota genuine ENOSPC errno and recovery after native close'
);
