/** Actual generic HTTP switching sockets, exercised against an owned raw TCP peer. */
import assert from 'node:assert/strict';
import http from 'node:http';
import net from 'node:net';
import { Buffer } from 'node:buffer';
import process from 'node:process';

const port = Number(process.argv[2]);
const headBytes = Buffer.from([0, 255, 127, 13, 10, 128]);
const timer = setTimeout(() => {
  throw new Error('HTTP upgrade fixture timed out');
}, 20000);
const agent = new http.Agent({ keepAlive: false, maxSockets: 1 });
const sockets = new Set();

const options = (path) => ({
  host: '127.0.0.1',
  port,
  path,
  agent,
  headers: { Connection: 'keep-alive, Upgrade', Upgrade: 'owned-protocol/1' }
});

function response(path, extra = {}) {
  return new Promise((resolve, reject) => {
    const req = http.get({ host: '127.0.0.1', port, path, agent, ...extra }, (res) => {
      const chunks = [];

      res.on('data', (data) => chunks.push(data));

      res.on('end', () =>
        resolve({ status: res.statusCode, bytes: Buffer.concat(chunks), request: req })
      );

      res.on('error', reject);
    });
    req.on('error', reject);
  });
}

function upgrade(path, signal, extra = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request({ ...options(path), signal, ...extra });
    let responseSeen = false;
    let removed = 0;

    const userTimeout = () => {};

    req.once('socket', (socket) => {
      socket.on('timeout', userTimeout);

      socket.on('agentRemove', () => {
        removed++;
      });
    });

    req.on('response', () => {
      responseSeen = true;
    });

    req.on('error', reject);

    req.once('upgrade', (res, socket, head) => {
      sockets.add(socket);
      assert.equal(res.statusCode, 101);
      assert.equal(responseSeen, false);
      assert.equal(removed, 1);
      assert.equal(socket.setEncoding, net.Socket.prototype.setEncoding);
      assert.equal(socket.listeners('timeout').includes(userTimeout), true);
      assert.deepEqual(head, headBytes);
      let received = Buffer.alloc(0);
      socket.unshift(head);

      socket.on('data', (data) => {
        received = Buffer.concat([received, data]);
        if (received.length >= headBytes.length) {
          assert.deepEqual(received.subarray(0, headBytes.length), headBytes);
          if (received.length === headBytes.length) {
            socket.write('ping');
          }
        }
        if (received.length === headBytes.length + 5) {
          assert.equal(received.subarray(headBytes.length).toString(), 'reply');
          resolve({ req, socket, removed: () => removed });
        }
      });

      socket.on('error', reject);
    });

    req.end();
  });
}

try {
  const abort = new AbortController();
  const switched = upgrade('/coalesced', abort.signal);
  const queued = response('/normal');
  const first = await switched;
  const ordinary = await queued;
  assert.equal(ordinary.status, 200);
  assert.equal(ordinary.bytes.toString(), 'ok!');
  assert.equal(first.socket.destroyed, false);
  assert.equal(first.removed(), 1);
  abort.abort();
  first.req.destroy();
  first.socket.write('still-owned');
  first.socket.destroy();
  console.log('PASS coalesced101 binary head, real socket ownership and Agent credit release');

  const split = await upgrade('/split');
  split.socket.destroy();
  console.log('PASS split switching headers preserve exact head without HTTP replay');

  const rejected = await response('/reject', {
    headers: { Connection: 'Upgrade', Upgrade: 'owned-protocol' }
  });
  assert.equal(rejected.status, 403);
  assert.equal(rejected.bytes.toString(), 'denied');
  console.log('PASS non101 Upgrade rejection remains an ordinary HTTP response');

  await new Promise((resolve, reject) => {
    const req = http.request(options('/no-listener'));
    req.on('error', reject);
    req.on('close', resolve);
    req.end();
  });
  assert.equal((await response('/normal')).bytes.toString(), 'ok!');
  console.log('PASS unobserved upgrade closes socket and ordinary requests remain functional');

  let returnedCalls = 0;
  const customUpgrade = await upgrade('/coalesced', undefined, {
    agent: undefined,

    createConnection(options, callback) {
      returnedCalls++;
      assert.equal(this, options);
      assert.equal(Object.getPrototypeOf(options), Object.prototype);
      assert.equal(options.path, undefined);
      const socket = net.createConnection(options);
      // Returning a socket and later reporting that same socket must not attach
      // HTTP listeners twice or destroy the already assigned transport.
      socket.once('connect', () => callback(null, socket));

      return socket;
    }
  });
  assert.equal(returnedCalls, 1);
  assert.equal(customUpgrade.socket.destroyed, false);
  customUpgrade.socket.destroy();

  let callbackCalls = 0;
  const asynchronous = await response('/normal', {
    agent: undefined,

    createConnection(options, callback) {
      callbackCalls++;
      assert.equal(this, options);
      assert.equal(Object.getPrototypeOf(options), Object.prototype);

      setTimeout(() => callback(null, net.createConnection(options)), 10);
    }
  });
  assert.equal(asynchronous.bytes.toString(), 'ok!');
  assert.equal(callbackCalls, 1);
  for (const selected of [agent, false]) {
    const explicit = await response('/normal', {
      agent: selected,

      createConnection() {
        throw new Error('Explicit Agent must take precedence');
      }
    });
    assert.equal(explicit.bytes.toString(), 'ok!');
  }
  console.log('PASS request connector return/callback and explicit Agent precedence');

  await new Promise((resolve, reject) => {
    const req = http.request({
      host: '127.0.0.1',
      port,
      path: '/normal',

      createConnection(options, callback) {
        setTimeout(() => {
          const socket = net.createConnection(options);

          socket.on('error', () => {});

          socket.once('close', resolve);
          callback(null, socket);
        }, 10);
      }
    });

    req.on('error', (error) => {
      if (error.code !== 'ECONNRESET') {
        reject(error);
      }
    });

    req.destroy();
  });
  const failure = new Error('Owned connector failure');
  await new Promise((resolve, reject) => {
    const req = http.request({
      host: '127.0.0.1',
      port,

      createConnection(_options, callback) {
        callback(failure);
      }
    });

    req.on('error', (error) => {
      try {
        assert.equal(error, failure);
        resolve();
      } catch (error) {
        reject(error);
      }
    });

    req.end();
  });
  console.log('PASS delayed connector cancellation closes late socket and preserves errors');

  for (const selected of [undefined, false, agent]) {
    let connectors = 0;
    const result = await response('/normal', {
      port: undefined,
      defaultPort: port,
      agent: selected,

      createConnection(options) {
        connectors++;
        assert.equal(this, options);
        assert.equal(Object.getPrototypeOf(options), Object.prototype);
        assert.equal(options.port, port);
        assert.equal(options.defaultPort, port);
        return net.createConnection(options);
      }
    });
    assert.equal(result.bytes.toString(), 'ok!');
    assert.equal(result.request.getHeader('host'), '127.0.0.1');
    assert.equal(connectors, selected === undefined ? 1 : 0);
  }
  console.log('PASS defaultPort selects real connection and Host default across Agent modes');
} finally {
  clearTimeout(timer);
  agent.destroy();
  for (const socket of sockets) {
    socket.destroy();
  }
}
