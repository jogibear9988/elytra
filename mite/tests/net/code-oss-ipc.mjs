/** Exercise unchanged Code-OSS channel framing over an actual local OS endpoint. */
import { pathToFileURL } from 'node:url';

const [tree, role, endpoint] = process.argv.slice(2);

const load = (file) => import(pathToFileURL(`${tree}/src/${file}`).href);

await load('nls.messages.js');
const { serve, connect } = await load('vs/base/parts/ipc/node/ipc.net.js');
const { VSBuffer } = await load('vs/base/common/buffer.js');
const { Event } = await load('vs/base/common/event.js');

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const bytes = VSBuffer.alloc(256 * 1024 + 37);
for (let i = 0; i < bytes.byteLength; i++) {
  bytes.buffer[i] = (i * 29 + 7) & 255;
}
const deadline = setTimeout(() => {
  console.error('Code-OSS local IPC timeout');
  process.exit(1);
}, 12000);

try {
  if (role === 'server') {
    const server = await serve(endpoint);
    let finish;
    const finished = new Promise((resolve) => {
      finish = resolve;
    });
    server.registerChannel('elytra-local-probe', {
      listen: () => Event.None,

      call: async (context, command, payload) => {
        check(context === 'elytra-client', 'real Code-OSS client context');
        if (command === 'finish') {
          // Wait for the reply and the client's genuine protocol disconnect.
          setTimeout(finish, 100);
          return true;
        }
        check(command === 'binary' && payload instanceof VSBuffer, 'VSBuffer channel decoding');
        check(payload.byteLength === bytes.byteLength, 'untruncated framed payload');
        for (let i = 0; i < bytes.byteLength; i++) {
          check(payload.buffer[i] === bytes.buffer[i], 'binary request integrity');
        }
        return payload;
      }
    });
    console.log('READY');
    await finished;
    server.dispose();
    console.log('PASS Code-OSS local IPC server');
  } else {
    check(role === 'client', 'known probe role');
    const client = await connect(endpoint, 'elytra-client');
    try {
      const channel = client.getChannel('elytra-local-probe');
      const reply = await channel.call('binary', bytes);
      check(reply instanceof VSBuffer && reply.byteLength === bytes.byteLength, 'binary response');
      for (let i = 0; i < bytes.byteLength; i++) {
        check(reply.buffer[i] === bytes.buffer[i], 'binary response integrity');
      }
      check((await channel.call('finish')) === true, 'second framed request/reply');
    } finally {
      client.dispose();
    }
    console.log('PASS Code-OSS local IPC client');
  }
} finally {
  clearTimeout(deadline);
}
