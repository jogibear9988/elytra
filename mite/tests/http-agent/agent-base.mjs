/** Exercise the unchanged, pinned package used by CodeOSS against real TCP sockets. */
import assert from 'node:assert/strict';
import http from 'node:http';
import https from 'node:https';
import net from 'node:net';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { once } from 'node:events';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';

const require = createRequire(resolve(process.argv[2], 'package.json'));
assert.equal(require('agent-base/package.json').version, '7.1.4');
// Match the untouched npm dependency in both the verified dependency tree and
// Code-OSS stage, rather than accepting a version label on an edited substitute.
assert.equal(
  createHash('sha256')
    .update(readFileSync(require.resolve('agent-base')))
    .digest('hex'),
  'c6503bd5e007db8b73fedf07b6eaaf4a94d5541953f0d06c8a17d1644c29a0c5'
);
const { Agent } = require('agent-base');
const payload = Buffer.from([0, 255, 7, 128]);
const server = http.createServer((req, res) => res.end(payload));
const agents = [];
const deadline = setTimeout(() => {
  throw Error('agent-base fixture deadline');
}, 15000);
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;

async function exchange(agent) {
  await new Promise((resolve, reject) => {
    const req = http.get({ host: '127.0.0.1', port, agent }, (res) => {
      const chunks = [];

      res.on('data', (chunk) => chunks.push(chunk));

      res.on('error', reject);

      res.on('end', () => {
        try {
          assert.deepEqual(Buffer.concat(chunks), payload);
          resolve();
        } catch (error) {
          reject(error);
        }
      });
    });
    req.on('error', reject);
  });
}

try {
  class Direct extends Agent {
    async connect(req, options) {
      const socket = net.createConnection({ host: options.host, port: options.port });
      await once(socket, 'connect');
      return socket;
    }
  }
  const direct = new Direct({ keepAlive: false, maxSockets: 1 });
  agents.push(direct);
  await Promise.all([exchange(direct), exchange(direct), exchange(direct)]);
  console.log('PASS pinned agent-base async actual TCP connection and finite admission');
  const fallback = new http.Agent({ keepAlive: false, maxSockets: 1 });
  class Delegated extends Agent {
    async connect() {
      return fallback;
    }
  }
  const delegated = new Delegated({ keepAlive: false, maxSockets: 1 });
  agents.push(fallback, delegated);
  await exchange(delegated);
  await exchange(delegated);
  assert.equal(delegated.totalSocketCount, 0);
  assert.equal(Object.keys(delegated.requests).length, 0);
  console.log(
    'PASS pinned agent-base delegates to genuine Agent without retaining outer reservation'
  );
  const secure = {
    host: 'localhost',
    port: 443,
    secureEndpoint: true,
    servername: 'other.example',
    rejectUnauthorized: true
  };
  assert.equal(direct.getName(secure), https.Agent.prototype.getName.call(direct, secure));
  assert.notEqual(direct.getName(secure), direct.getName({ ...secure, rejectUnauthorized: false }));
  console.log('PASS pinned agent-base borrows HTTPS policy-sensitive getName');
} finally {
  clearTimeout(deadline);
  for (const agent of agents) {
    agent.destroy();
  }
  server.closeAllConnections();
  await new Promise((resolve) => server.close(resolve));
}
