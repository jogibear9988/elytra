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
    controllers: [],
    proxyQueries: 0,
    protocolCalls: [],
    protocolChecks: [],
    registry: null
  };
  const native = {
    networkHasProxy() {
      state.proxyQueries++;
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
    transport,
    () => state.registry,
    () => state.requestPolicy
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

function protocols(state, handled, responder) {
  state.registry = {
    canHandle(address) {
      state.protocolChecks.push(address);
      return handled(new URL(address));
    },

    async fetch(address, init) {
      state.protocolCalls.push({ address, ...init });
      return responder(address, init);
    }
  };
}

{
  const { net, state, api } = fixture();
  state.proxy = true;

  protocols(
    state,
    (url) => url.protocol === 'app:',
    () => new Response('handled')
  );

  const response = await net.fetch('app://one.test/resource', {
    bypassCustomProtocolHandlers: false
  });
  check(
    (await response.text()) === 'handled' && state.proxyQueries === 0 && !state.calls.length,
    'Handled custom URL neither consults OS proxy nor reaches native network transport'
  );
  check(
    state.protocolCalls.length === 1 &&
      state.protocolChecks.every((address) => address.startsWith('app://one.test/')),
    'Registry queried using full canonical URL'
  );
  for (const value of [null, 1, 'false', {}]) {
    await rejects(
      net.fetch('app://one.test/resource', { bypassCustomProtocolHandlers: value }),
      'ERR_INVALID_ARG_TYPE'
    );
  }
  state.proxy = false;

  protocols(
    state,
    (url) => url.protocol === 'http:',
    () => new Response('controlled registry')
  );

  state.responses.push(new Response('bypassed'));
  const before = state.protocolCalls.length;
  check(
    (await (
      await net.fetch('http://one.test/bypass', { bypassCustomProtocolHandlers: true })
    ).text()) === 'bypassed',
    'Explicit bypass reaches network transport'
  );
  check(
    state.protocolCalls.length === before && state.calls.length === 1 && state.proxyQueries === 1,
    'Bypass cannot invoke registry and applies native network policy'
  );
  check(
    (await (
      await net.fetch('http://one.test/handled', { bypassCustomProtocolHandlers: false })
    ).text()) === 'controlled registry',
    'False enables controlled registry lookup'
  );
  check(state.proxyQueries === 1, 'Handled URL does not inherit network proxy check');
  api.stop();
  console.log(
    'PASS custom registry full URL routing proxy isolation explicit bypass and strict flag types'
  );
}

{
  const { net, state, api } = fixture();
  const statuses = [
    new Response(null, { status: 302, headers: { location: 'https://network.test/final' } })
  ];

  protocols(
    state,
    (url) => url.protocol === 'app:',
    () => statuses.shift() ?? new Response('custom final')
  );

  state.responses.push(new Response('network final'));
  const fromCustom = await net.fetch('app://source.test/start', {
    headers: { authorization: 'secret', cookie: 'private=1' }
  });
  check(
    (await fromCustom.text()) === 'network final' &&
      fromCustom.redirected &&
      fromCustom.url === 'https://network.test/final',
    'Custom-to-HTTP redirect changes actual transport'
  );
  check(
    state.calls.length === 1 &&
      state.proxyQueries === 1 &&
      !state.calls[0].headers.has('authorization') &&
      !state.calls[0].headers.has('cookie'),
    'Network hop applies proxy policy and strips cross-authority credentials'
  );
  state.responses.push(
    new Response(null, { status: 302, headers: { location: 'app://destination.test/final' } })
  );
  const fromHTTP = await net.fetch('https://network.test/start');
  check(
    (await fromHTTP.text()) === 'custom final' &&
      fromHTTP.url === 'app://destination.test/final' &&
      fromHTTP.redirected,
    'HTTP-to-custom redirect rechecks handler registry'
  );
  check(
    state.proxyQueries === 2 && state.calls.length === 2 && state.protocolCalls.length === 2,
    'Only network hops consult native proxy'
  );
  const observed = [];

  protocols(
    state,
    (url) => url.protocol === 'app:',
    (address, init) => {
      observed.push({ address, headers: new Headers(init.headers) });
      return observed.length === 1
        ? new Response(null, { status: 302, headers: { location: 'app://other.test/final' } })
        : new Response('different authority');
    }
  );

  const crossHost = await net.fetch('app://first.test/start', {
    headers: {
      authorization: 'Bearer private',
      cookie: 'private=1',
      'proxy-authorization': 'Basic private'
    }
  });
  await crossHost.text();
  check(
    observed[0].headers.has('authorization') &&
      ['authorization', 'cookie', 'proxy-authorization'].every(
        (name) => !observed[1].headers.has(name)
      ),
    'Distinct custom hosts with opaque null origins never inherit sensitive headers'
  );
  check(
    state.proxyQueries === 2,
    'Custom-to-custom redirect remains isolated from OS network provider'
  );
  api.stop();
  console.log(
    'PASS per-hop custom HTTP redirects and opaque-origin cross-host credential isolation'
  );
}

{
  const { net, state, api, error } = fixture();
  let abortReason;

  protocols(
    state,
    (url) => url.protocol === 'app:',
    (_address, init) =>
      new Promise((_resolve, reject) => {
        const abort = () => {
          abortReason = init.signal.reason;
          reject(abortReason);
        };

        if (init.signal.aborted) {
          abort();
        } else {
          init.signal.addEventListener('abort', abort, { once: true });
        }
      })
  );

  const controller = new AbortController();
  const reason = error('OWN_CUSTOM_ABORT', 'Caller custom fetch cancellation');
  const pending = net.fetch('app://first.test/pending', { signal: controller.signal });
  const rejected = rejects(pending, reason.code);
  controller.abort(reason);
  await rejected;
  check(
    abortReason === reason && state.proxyQueries === 0 && !state.calls.length,
    'Owned protocol fetch receives original abort reason without network admission'
  );
  let bodyCancel;

  protocols(
    state,
    (url) => url.protocol === 'app:',
    () =>
      new Response(
        new ReadableStream(
          {
            cancel(value) {
              bodyCancel = value;
            }
          },
          { highWaterMark: 0 }
        )
      )
  );

  const response = await net.fetch('app://first.test/body');
  await response.body.cancel(reason);
  check(bodyCancel === reason, 'Public response cancellation reaches handled body reader');

  protocols(
    state,
    (url) => url.protocol === 'app:',
    (_address, init) =>
      new Promise((_resolve, reject) => {
        init.signal.addEventListener(
          'abort',
          () => {
            abortReason = init.signal.reason;
            reject(abortReason);
          },
          { once: true }
        );
      })
  );

  const shutdown = rejects(net.fetch('app://first.test/shutdown'), 'ERR_FIREFLY_APP_EXITED');
  api.stop();
  await shutdown;
  check(
    abortReason.code === 'ERR_FIREFLY_APP_EXITED' && state.stops === 1,
    'Shutdown aborts owned protocol request before native network provider stop'
  );
  console.log('PASS handled fetch caller abort body cancellation and shutdown ownership cleanup');
}

{
  const { net, state, api } = fixture();
  const snapshots = [];
  let sequence = 0;
  state.requestPolicy = {
    allocateId: () => (sequence += 2),

    async intercept(stage, details) {
      if (stage !== 'before') {
        return {};
      }
      snapshots.push(details);
      const fields = { ...details.requestHeaders, 'x-policy-added': ['real-value'] };
      delete fields['x-policy-remove'];
      return { requestHeaders: fields };
    }
  };
  state.responses.push(new Response('fetch-body'), new Response('request-body'));
  const response = await net.fetch('http://first.test/headers', {
    headers: { 'x-original': 'fetch-original', 'x-policy-remove': 'remove' }
  });
  check((await response.text()) === 'fetch-body', 'Fetch retains real response body');
  const request = net.request({
    url: 'http://first.test/request-headers',
    headers: {
      'x-original': 'request-original',
      'x-policy-remove': 'remove'
    }
  });
  const received = new Promise((resolve, reject) => {
    request.on('error', reject);

    request.on('response', async (incoming) => {
      try {
        let body = '';
        for await (const chunk of incoming) {
          body += chunk.toString();
        }
        resolve(body);
      } catch (failure) {
        reject(failure);
      }
    });
  });
  request.end();
  check((await received) === 'request-body', 'ClientRequest retains real response stream');
  check(
    snapshots.length === 2 &&
      snapshots[0].requestHeaders['x-original'][0] === 'fetch-original' &&
      snapshots[1].requestHeaders['x-original'][0] === 'request-original',
    'Both APIs expose actual pre-transport header snapshots'
  );
  check(
    state.calls.every(
      (call) =>
        call.headers.get('x-policy-added') === 'real-value' && !call.headers.has('x-policy-remove')
    ),
    'Both APIs apply policy additions and removals to actual transport headers'
  );
  check(
    snapshots.every((details) => details.requestHeaders['x-policy-remove'][0] === 'remove'),
    'Applying decisions does not mutate the private original snapshot'
  );
  api.stop();
  console.log('PASS main net request header snapshots and genuine transport header mutations');
}

console.log('DONE controlled net policy');
