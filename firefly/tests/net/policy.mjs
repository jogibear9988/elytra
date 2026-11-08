/** Actual net facade with controlled transport/native snapshots; no network connections are opened. */
import create from '../../core/net/index.mjs';
import { Readable, Writable } from 'node:stream';
import { EventEmitter, once } from 'node:events';

const captured = {
  Request,
  Response,
  Headers,
  URL,
  AbortController,
  ReadableStream,
  Readable,
  Writable,
  Buffer,
  EventEmitter
};

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(failure?.code === code, 'Expected ' + code + ', got ' + failure?.code);
  return failure;
}

async function rejects(promise, code) {
  let failure;
  try {
    await promise;
  } catch (error) {
    failure = error;
  }
  check(failure?.code === code, 'Expected rejection ' + code + ', got ' + failure?.code);
  return failure;
}

function fixture() {
  const state = {
    ready: true,
    exited: false,
    proxy: false,
    online: false,
    stops: 0,
    calls: [],
    responses: [],
    controllers: []
  };
  const native = {
    networkHasProxy() {
      state.onProxy?.();
      return state.proxy;
    },

    networkIsOnline() {
      state.onOnline?.();
      return state.online;
    },

    networkStop() {
      state.stops++;
    }
  };
  const transport = {
    ...captured,
    abortHooks: {
      reason: (signal) => signal.reason,

      subscribe(signal, callback) {
        if (signal.aborted) {
          callback();
          return () => {};
        }
        signal.addEventListener('abort', callback);
        return () => signal.removeEventListener('abort', callback);
      }
    },

    async fetch(address, init) {
      state.calls.push({ address, ...init });
      state.onFetch?.(init);
      const response = state.responses.shift();
      if (!response) {
        throw Error('Unexpected transport request');
      }
      return typeof response === 'function' ? response(init) : response;
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const api = create(
    native,
    () => state.ready,
    () => state.exited,
    error,
    (name) => {
      throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
    },
    transport
  );
  return { api, state, net: api.net, error };
}

{
  const { net, state, api } = fixture();
  state.ready = false;
  check(net.isOnline() === false && net.online === false, 'Online snapshot has no ready gate');

  fails(() => net.request('http://first.test'), 'ERR_FIREFLY_NOT_READY');

  await rejects(net.fetch('http://first.test'), 'ERR_FIREFLY_NOT_READY');
  state.ready = true;
  for (const options of [
    { session: {} },
    { partition: '' },
    { useSessionCookies: false },
    { credentials: 'include' },
    { rejectUnauthorized: false },
    { auth: 'user:secret' },
    { agent: {} },
    { priority: 'high' }
  ]) {
    fails(
      () => net.request({ url: 'https://first.test', ...options }),
      'ERR_FIREFLY_UNSUPPORTED_API'
    );
  }
  state.proxy = true;
  await rejects(net.fetch('http://first.test'), 'ERR_FIREFLY_UNSUPPORTED_API');
  check(!state.calls.length, 'Configured proxy cannot silently become a direct request');
  state.proxy = false;
  state.online = 'true';

  fails(() => net.isOnline(), 'ERR_FIREFLY_NET_DATA');

  api.stop();
  api.stop();
  check(state.stops === 1, 'Provider stops once');

  fails(() => net.isOnline(), 'ERR_FIREFLY_APP_EXITED');

  console.log(
    'PASS readiness native online snapshots explicit session gaps proxy refusal and shutdown'
  );
}

{
  const { net, state, api } = fixture();
  state.responses.push(
    new Response(null, { status: 302, headers: { location: 'http://second.test/next' } }),
    new Response('payload', { status: 201 })
  );
  const response = await net.fetch('http://first.test/start', {
    headers: { authorization: 'Bearer private', cookie: 'secret=1' }
  });
  check(
    response.status === 201 && response.url === 'http://second.test/next' && response.redirected,
    'Actual final transport response metadata'
  );
  check(
    !state.calls[1].headers.has('authorization') && !state.calls[1].headers.has('cookie'),
    'Cross-origin redirect strips sensitive headers'
  );
  check(
    state.calls.every((call) => call.redirect === 'manual'),
    'Each hop is independently proxy guarded'
  );
  const clone = response.clone();
  check(
    clone.url === response.url &&
      (await clone.text()) === 'payload' &&
      (await response.text()) === 'payload',
    'Body cloning preserves actual metadata'
  );
  state.responses.push(new Response(null, { status: 302, headers: { location: '/next' } }));
  await rejects(net.fetch('http://first.test', { redirect: 'error' }), 'ERR_FIREFLY_NET_REDIRECT');
  state.responses.push(new Response(null, { status: 307, headers: { location: '/again' } }));
  await rejects(
    net.fetch('http://first.test', { method: 'POST', body: 'once' }),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
  api.stop();
  console.log('PASS guarded fetch redirects sensitive-header stripping response bodies and clones');
}

{
  const { net, state, api } = fixture();
  state.responses.push(new Response('answer', { status: 200, headers: { 'x-result': 'real' } }));
  const request = net.request({
    url: 'http://first.test',
    method: 'POST',
    agent: null,
    rejectUnauthorized: true
  });
  request.setHeader('X-List', ['a', 'b']);
  check(request.getHeader('x-list').join(',') === 'a,b', 'Owned request headers');
  request.removeHeader('X-List');
  request.setHeader('Content-Type', 'text/plain');
  const original = Buffer.from('payload');
  request.write(original);
  original.fill(120);

  fails(() => request.setHeader('Late', 'value'), 'ERR_FIREFLY_NET_HEADERS_SENT');

  const reply = once(request, 'response');
  const closing = once(request, 'close');
  request.end();
  const [response] = await reply;
  const chunks = [];

  response.on('data', (chunk) => chunks.push(chunk));

  await closing;
  check(
    state.calls[0].body.toString() === 'payload' && Buffer.concat(chunks).toString() === 'answer',
    'Upload snapshot and genuine response stream'
  );
  check(
    response.statusCode === 200 && response.headers['x-result'] === 'real',
    'Response status and headers'
  );

  fails(() => response.rawHeaders, 'ERR_FIREFLY_UNSUPPORTED_API');

  const unsupported = net.request('http://first.test');

  fails(() => {
    unsupported.chunkedEncoding = true;
  }, 'ERR_FIREFLY_UNSUPPORTED_API');

  unsupported.abort();
  api.stop();
  console.log(
    'PASS buffered Electron request writes headers real response streams and explicit metadata gaps'
  );
}

{
  const { net, state, api } = fixture();
  state.responses.push(
    new Response(null, { status: 302, headers: { location: '/next' } }),
    new Response('followed')
  );
  const request = net.request({ url: 'http://first.test', redirect: 'manual' });

  request.on('redirect', (status, method, address) => {
    check(
      status === 302 && method === 'GET' && address === 'http://first.test/next',
      'Manual redirect event'
    );
    request.followRedirect();
  });

  const closing = once(request, 'close');

  request.on('response', (response) => response.resume());

  request.end();
  await closing;
  check(state.calls.length === 2, 'Synchronous manual follow admits next request');
  let aborted;

  state.responses.push(
    (init) =>
      new Promise((resolve, reject) => {
        init.signal.addEventListener('abort', () => {
          aborted = true;
          reject(init.signal.reason);
        });
      })
  );

  const pending = net.fetch('http://first.test/pending');
  const failure = rejects(pending, 'ERR_FIREFLY_APP_EXITED');
  api.stop();
  await failure;
  check(aborted && state.stops === 1, 'Shutdown aborts pending transport before provider teardown');
  console.log('PASS manual redirects request disposal and pending fetch shutdown cancellation');
}

{
  const { net, state, api } = fixture();
  const failures = [];
  let aborts = 0;
  for (let index = 0; index < 2; index++) {
    state.responses.push(
      (init) =>
        new Promise((resolve, reject) => {
          init.signal.addEventListener('abort', () => {
            aborts++;
            reject(init.signal.reason);
          });
        })
    );

    failures.push(rejects(net.fetch('http://first.test/' + index), 'ERR_FIREFLY_APP_EXITED'));
  }
  const replaced = net.request('http://first.test/replaced');
  let publicCalls = 0;

  replaced.destroy = () => {
    publicCalls++;
    throw Error('Public destroy must not control shutdown');
  };

  const throwing = net.request('http://first.test/throwing');

  throwing.on('error', () => {});

  throwing._destroy = () => {
    throw Error('Hostile public stream hook');
  };

  const remaining = net.request('http://first.test/remaining');
  api.stop();
  await Promise.all(failures);
  check(
    publicCalls === 0 && replaced.destroyed && remaining.destroyed,
    'Captured destroy ignores replaceable public method'
  );
  check(
    aborts === 2 && state.stops === 1,
    'Throwing hook cannot prevent other aborts or provider shutdown'
  );
  api.stop();
  console.log(
    'PASS shutdown survives replaced destroy throwing hooks and revokes every owned exchange'
  );
}

{
  const { net, state, api } = fixture();
  const originalFrom = Readable.from;
  const originalFromWeb = Readable.fromWeb;

  Readable.from = () => {
    throw Error('Mutable Readable.from reached');
  };

  Readable.fromWeb = () => {
    throw Error('Mutable Readable.fromWeb reached');
  };

  try {
    for (const response of [new Response('body'), new Response(null, { status: 204 })]) {
      state.responses.push(response);
      const request = net.request('http://first.test/captured');

      request.on('response', (incoming) => incoming.resume());

      const closing = once(request, 'close');
      request.end();
      await closing;
    }
  } finally {
    Readable.from = originalFrom;
    Readable.fromWeb = originalFromWeb;
    api.stop();
  }
  console.log('PASS captured response adapters survive public Readable static replacement');
}

console.log('DONE controlled net policy');
