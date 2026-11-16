/** Real loopback framing and native connection admission, compared with Node. */
import assert from 'node:assert/strict';
import http from 'node:http';
import net from 'node:net';
import process from 'node:process';
const { Agent } = http;
const agents = [];
const servers = [];
const requests = [];
const held = new Map();
const waiters = new Map();
const seen = [];
const payload = Buffer.from([0, 255, 17, 128, 1, 3]);
const timer = setTimeout(() => {
  throw new Error('HTTP Agent fixture timed out');
}, 20000);

function makeAgent(Type = Agent, options = {}) {
  const agent = new Type({ keepAlive: false, ...options });
  agents.push(agent);
  return agent;
}

function arrival(path) {
  if (held.has(path)) {
    return Promise.resolve();
  }
  return new Promise((resolve) => waiters.set(path, resolve));
}

async function server() {
  const value = http.createServer((req, res) => {
    seen.push(req.url);
    if (req.url.startsWith('/held-')) {
      held.set(req.url, res);
      waiters.get(req.url)?.();
      waiters.delete(req.url);
    } else {
      res.end(payload);
    }
  });
  servers.push(value);
  await new Promise((resolve) => value.listen(0, '127.0.0.1', resolve));
  return value.address().port;
}

function request(port, agent, path, options = {}) {
  const counts = { error: 0, close: 0, socket: 0 };
  let resolveOutcome;
  const outcome = new Promise((resolve) => {
    resolveOutcome = resolve;
  });
  const req = http.get({ host: '127.0.0.1', port, agent, path, ...options }, (res) => {
    const chunks = [];

    res.on('data', (chunk) => chunks.push(chunk));

    res.on('error', (error) => resolveOutcome({ error }));

    res.on('end', () => resolveOutcome({ body: Buffer.concat(chunks) }));
  });

  req.on('error', (error) => {
    counts.error++;
    resolveOutcome({ error });
  });

  req.on('socket', () => {
    counts.socket++;
  });

  const closed = new Promise((resolve) =>
    req.on('close', () => {
      counts.close++;
      resolve();
    })
  );
  const value = { req, outcome, closed, counts };
  requests.push(value);
  return value;
}

async function success(value) {
  const result = await value.outcome;
  assert.equal(result.error, undefined);
  assert.deepEqual(result.body, payload);
  await value.closed;
  assert.deepEqual(value.counts, { error: 0, close: 1, socket: 1 });
}

try {
  const port = await server();
  const agent = makeAgent(Agent, { maxSockets: 1 });
  const first = request(port, agent, '/held-origin');
  await arrival('/held-origin');
  const second = request(port, agent, '/second');
  const third = request(port, agent, '/third');
  const key = agent.getName({ host: '127.0.0.1', port });
  assert.equal(agent.sockets[key].length, 1);
  assert.equal(agent.requests[key].length, 2);
  held.get('/held-origin').end(payload);
  await Promise.all([first, second, third].map(success));
  assert.deepEqual(seen.slice(0, 3), ['/held-origin', '/second', '/third']);
  console.log('PASS Agent per-origin FIFO admission and actual response bytes');

  const otherPort = await server();
  const total = makeAgent(Agent, { maxSockets: Infinity, maxTotalSockets: 1 });
  const active = request(port, total, '/held-total');
  await arrival('/held-total');
  const waiting = request(otherPort, total, '/other-origin');
  assert.equal(total.totalSocketCount, 1);
  assert.equal(seen.includes('/other-origin'), false);
  held.get('/held-total').end(payload);
  await Promise.all([active, waiting].map(success));
  console.log('PASS Agent maxTotalSockets gates distinct real origins');

  class AsyncConnection extends Agent {
    createConnection(options, callback) {
      setImmediate(() =>
        callback(null, net.createConnection({ host: options.host, port: options.port }))
      );
    }
  }
  await success(request(port, makeAgent(AsyncConnection), '/async-connection'));
  class Connected extends Agent {
    createSocket(req, options, callback) {
      const socket = net.createConnection({ host: options.host, port: options.port });

      const failed = (error) => callback(error);

      socket.once('error', failed);

      socket.once('connect', () => {
        socket.removeListener('error', failed);
        this.current = socket;
        super.createSocket(req, options, callback);
      });
    }

    createConnection() {
      const result = this.current;
      this.current = null;
      return result;
    }
  }
  await success(request(port, makeAgent(Connected), '/already-connected'));
  console.log('PASS asynchronous createConnection and already-connected createSocket subclasses');

  const aborting = makeAgent(Agent, { maxSockets: 1 });
  const blocking = request(port, aborting, '/held-abort');
  await arrival('/held-abort');
  const signal = new AbortController();
  const reason = { owned: 'queued-abort' };
  const cancelled = request(port, aborting, '/never-queued', { signal: signal.signal });
  signal.abort(reason);
  held.get('/held-abort').end(payload);
  assert.equal((await cancelled.outcome).error.code, 'ABORT_ERR');
  await cancelled.closed;
  await success(blocking);
  assert.equal(seen.includes('/never-queued'), false);
  assert.equal(cancelled.counts.error, 1);
  assert.equal(cancelled.counts.close, 1);
  class Late extends Agent {
    createConnection(options, callback) {
      this.optionsForLate = options;
      this.pending = callback;
    }
  }
  const late = makeAgent(Late);
  const delayed = request(port, late, '/never-late');
  const marker = new Error('owned pending abort');
  delayed.req.destroy(marker);
  const raw = net.createConnection({ host: '127.0.0.1', port });
  const rawClosed = new Promise((resolve) => raw.once('close', resolve));
  await new Promise((resolve, reject) => {
    raw.once('connect', resolve);
    raw.once('error', reject);
  });
  late.pending(null, raw);
  assert.equal((await delayed.outcome).error, marker);
  await Promise.all([delayed.closed, rawClosed]);
  assert.equal(raw.destroyed, true);
  assert.equal(seen.includes('/never-late'), false);
  assert.equal(delayed.counts.error, 1);
  assert.equal(delayed.counts.close, 1);
  console.log(
    'PASS queued abort and late socket admission close exactly once without endpoint hit'
  );

  const connectorFailure = new Error('owned connector failure');
  class Failing extends Agent {
    createConnection(options, callback) {
      setImmediate(() => callback(connectorFailure));
    }
  }
  const failing = makeAgent(Failing);
  const failed = request(port, failing, '/never-failed');
  assert.equal((await failed.outcome).error, connectorFailure);
  await failed.closed;
  assert.equal(failed.counts.error, 1);
  assert.equal(failed.counts.close, 1);
  assert.equal(failing.totalSocketCount, 0);
  class Duplicate extends Agent {
    createConnection(options, callback) {
      const socket = net.createConnection({ host: options.host, port: options.port });
      callback(null, socket);
      callback(null, socket);
      return socket;
    }
  }
  if (process.versions.mite) {
    await success(request(port, makeAgent(Duplicate), '/duplicate-callback'));
  }
  console.log('PASS connector failure preserves request/socket ownership');

  if (process.versions.mite) {
    // Mite intentionally cancels unresolved admissions on Agent.destroy().
    const pendingAgent = makeAgent(Late);
    const pendingRequest = request(port, pendingAgent, '/never-destroyed-pending');
    pendingAgent.destroy();
    assert.equal((await pendingRequest.outcome).error.code, 'ECONNRESET');
    await pendingRequest.closed;
    const supplied = net.createConnection({ host: '127.0.0.1', port });
    const suppliedClosed = new Promise((resolve) => supplied.once('close', resolve));
    await new Promise((resolve, reject) => {
      supplied.once('connect', resolve);
      supplied.once('error', reject);
    });
    pendingAgent.pending(null, supplied);
    await suppliedClosed;
    assert.equal(seen.includes('/never-destroyed-pending'), false);
    assert.equal(pendingAgent.totalSocketCount, 0);
  }

  const destroyed = makeAgent();
  const inFlight = request(port, destroyed, '/held-destroy');
  await arrival('/held-destroy');
  destroyed.destroy();
  assert.ok((await inFlight.outcome).error);
  await inFlight.closed;
  await success(request(port, destroyed, '/after-destroy'));
  console.log('PASS destroy closes real active sockets and later requests can use the Agent');
} finally {
  clearTimeout(timer);
  for (const request of requests) {
    if (!request.req.destroyed) {
      request.req.destroy();
    }
  }
  for (const agent of agents) {
    agent.destroy();
  }
  for (const server of servers) {
    server.closeAllConnections();
    await new Promise((resolve) => server.close(resolve));
  }
}
