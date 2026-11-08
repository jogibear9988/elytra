/** Actual public registry/controller with a controlled native wire; no renderer routing claim. */
import create from '../../core/protocol/index.mjs';

const error = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const tick = () => new Promise((resolve) => setTimeout(resolve, 0));

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
}

async function rejects(promise, code) {
  let caught;
  try {
    await promise;
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
}

const signalGet = Object.getOwnPropertyDescriptor(AbortSignal.prototype, 'aborted').get;
const signalReason = Object.getOwnPropertyDescriptor(AbortSignal.prototype, 'reason').get;
const add = AbortSignal.prototype.addEventListener;
const remove = AbortSignal.prototype.removeEventListener;
const transport = {
  Request,
  Response,
  Headers,
  URL,
  AbortController,
  AbortSignal,
  ReadableStream,
  Buffer,
  abortHooks: {
    reason: (signal) => signalReason.call(signal),

    subscribe(signal, callback) {
      if (signalGet.call(signal)) {
        callback();
      } else {
        add.call(signal, 'abort', callback, { once: true });
      }
      return () => remove.call(signal, 'abort', callback);
    }
  }
};

function fixture() {
  const state = {
    ready: false,
    generation: 18446744073709551600n,
    heads: [],
    chunks: [],
    removed: [],
    routes: new Map()
  };
  const native = {
    registerSchemes() {},

    protocolRegisterResponse(name) {
      const generation = String(++state.generation);
      state.routes.set(name, { generation, modern: true });
      state.onRegister?.(name);
      return generation;
    },

    protocolRegisterFile(name) {
      const generation = String(++state.generation);
      state.routes.set(name, { generation, modern: false });
      return generation;
    },

    protocolUnregisterFile(name) {
      state.removed.push(name);
      return state.routes.delete(name);
    },

    protocolResponseHead(wire) {
      state.heads.push(JSON.parse(wire));
      return state.headResult ?? true;
    },

    protocolResponseChunk(token, bytes, done) {
      state.chunks.push({ token, bytes: Buffer.from(bytes), done });
      return true;
    },

    protocolFileReply(wire) {
      state.fileReply = JSON.parse(wire);
      return true;
    }
  };
  const api = create(native, () => state.ready, error, unsupported, transport);
  api.protocol.registerSchemesAsPrivileged(
    ['app', 'remote'].map((scheme) => ({
      scheme,
      privileges: { standard: true, supportFetchAPI: true }
    }))
  );
  return { api, protocol: api.protocol, state, native };
}

function request(f, scheme, token = '18446744073709551615', method = 'GET') {
  const packet = {
    token,
    generation: f.state.routes.get(scheme).generation,
    scheme,
    modern: true,
    method,
    url: scheme + '://bundle/a'
  };
  f.api.dispatch('protocol-request', packet);
  return packet;
}

{
  const f = fixture();

  fails(() => f.protocol.handle('app', () => new Response()), 'ERR_FIREFLY_NOT_READY');

  f.state.ready = true;

  fails(() => f.protocol.handle('missing', () => new Response()), 'ERR_FIREFLY_UNSUPPORTED_API');

  f.protocol.handle('app', () => new Response('modern'));

  check(
    f.protocol.isProtocolHandled('app') === true && f.api.canLoad('app'),
    'Shared modern handled/load query'
  );

  fails(
    () => f.protocol.handle('app', () => new Response()),
    'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED'
  );

  check(f.protocol.registerFileProtocol('app', () => {}) === false, 'Legacy cannot replace modern');
  check(
    f.protocol.unregisterProtocol('app') === true && !f.protocol.isProtocolHandled('app'),
    'Shared unregister modern'
  );
  check(
    f.protocol.registerFileProtocol('app', (_request, callback) => callback('/tmp/owned')),
    'Legacy registered'
  );

  fails(
    () => f.protocol.handle('app', () => new Response()),
    'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED'
  );

  f.protocol.unhandle('app');
  check(f.protocol.isProtocolHandled('app'), 'Modern unhandle cannot remove legacy');
  f.protocol.unregisterProtocol('app');

  f.protocol.handle('file', () => Response.error());

  check(
    f.api.canHandle('file:///private/file') && !f.api.canHandle('https://example.test'),
    'Builtin file interceptor distinct from privileged scheme registration'
  );
  f.api.stop();
  console.log(
    'PASS public readiness scheme authority legacy modern collisions and builtin file interception'
  );
}

{
  const f = fixture();
  f.state.ready = true;

  f.protocol.handle('app', async (request) => {
    check(
      request instanceof Request && request.url === 'app://bundle/a',
      'Genuine handler Request'
    );
    return new Response(Uint8Array.of(0, 255, 1), {
      status: 404,
      headers: { 'content-type': 'application/octet-stream', 'content-length': '3' }
    });
  });

  const packet = request(f, 'app');
  await tick();
  check(
    f.state.heads[0].token === packet.token && f.state.heads[0].head.status === 404,
    'Opaque u64 token preserved and response metadata'
  );
  for (const credit of [2, 2, 2]) {
    f.api.dispatch('protocol-pull', { ...packet, credit });
    await tick();
  }
  check(
    f.state.chunks.length === 3 &&
      f.state.chunks[0].bytes.equals(Buffer.from([0, 255])) &&
      f.state.chunks[1].bytes[0] === 1 &&
      f.state.chunks[2].done,
    'Credit-controlled real byte stream and EOF'
  );
  f.api.dispatch('protocol-pull', { ...packet, credit: 2 });
  await tick();
  check(f.state.chunks.length === 3, 'EOF retires native token ownership');
  f.api.stop();
  console.log(
    'PASS opaque native identities async metadata bounded demand binary chunks and EOF retirement'
  );
}

{
  const f = fixture();
  f.state.ready = true;

  f.protocol.handle('app', () => new Response('stale'));

  const packet = request(f, 'app');
  f.protocol.unhandle('app');

  f.protocol.handle('app', () => new Response('fresh'));

  f.api.dispatch('protocol-request', packet);
  await tick();
  check(
    f.state.heads.every((head) => head.error === true),
    'Old registration generation cannot invoke replacement'
  );
  const next = request(f, 'app', '18446744073709551614');
  await tick();
  f.api.dispatch('protocol-cancel', { ...next, generation: packet.generation });
  f.api.dispatch('protocol-pull', { ...next, credit: 65536 });
  await tick();
  check(
    f.state.chunks.at(-1).bytes.toString() === 'fresh',
    'Stale cancel cannot retire current generation'
  );
  f.api.dispatch('protocol-cancel', next);

  f.protocol.handle('file', () => Response.error());

  request(f, 'file', '18446744073709551613');
  await tick();
  check(
    f.state.heads.at(-1).error === true,
    'Response.error denies native request before file access'
  );
  f.api.stop();
  console.log(
    'PASS generations unregister cancellation stale isolation and explicit native network-error denial'
  );
}

{
  const f = fixture();
  f.state.ready = true;

  f.protocol.handle(
    'remote',
    (request) =>
      new Response(request.headers.get('x-test'), {
        status: 500,
        headers: { 'content-type': 'text/plain' }
      })
  );

  const response = await f.api.fetch('remote://window:1/a', {
    headers: { 'x-test': 'main-fetch' }
  });
  check(
    response instanceof Response &&
      response.status === 500 &&
      (await response.text()) === 'main-fetch',
    'Main fetch returns genuine streamed Response'
  );

  f.protocol.handle('file', () => Response.error());

  await rejects(f.api.fetch('file:///private'), 'ERR_FIREFLY_PROTOCOL_NETWORK');
  f.protocol.unhandle('remote');
  await rejects(f.api.fetch('remote://window:1/a'), 'ERR_FIREFLY_PROTOCOL_UNHANDLED');
  f.api.stop();
  console.log(
    'PASS internal default-session fetch genuine Request Response headers status MIME and file deny'
  );
}

{
  const f = fixture();
  f.state.ready = true;
  let signal;
  let sourceCanceled = 0;

  f.protocol.handle('app', (request) => {
    signal = request.signal;
    return new Response(
      new ReadableStream(
        {
          cancel() {
            sourceCanceled++;
          }
        },
        { highWaterMark: 0 }
      )
    );
  });

  const abort = new AbortController();
  const response = await f.api.fetch('app://bundle/a', { signal: abort.signal });
  const reason = error('OWN_ABORT', 'Caller aborted');
  abort.abort(reason);
  await rejects(response.text(), 'OWN_ABORT');
  check(
    signal.aborted && signal.reason === reason && sourceCanceled === 1,
    'Caller cancellation reaches genuine request and owned reader'
  );
  const pre = new AbortController();
  pre.abort(reason);
  await rejects(f.api.fetch('app://bundle/a', { signal: pre.signal }), 'OWN_ABORT');
  f.api.stop();
  // Reenter at genuine typed-byte result creation, after the controller has read/retired
  // its own body but before the public fetch/body continuation can publish that result.
  const OriginalBytes = globalThis.Uint8Array;
  const constructor = Object.getOwnPropertyDescriptor(OriginalBytes.prototype, 'constructor');
  for (const mode of ['HEAD', 'stream']) {
    for (const teardown of ['unhandle', 'stop']) {
      let armed = false;
      let copies = 0;
      let revoked = false;
      let g;

      function ObservedBytes(...args) {
        const bytes = new OriginalBytes(...args);
        if (
          armed &&
          ((mode === 'HEAD' && !bytes.byteLength) || (mode === 'stream' && ++copies === 2))
        ) {
          armed = false;
          revoked = true;
          if (teardown === 'stop') {
            g.api.stop();
          } else {
            g.protocol.unhandle('app');
          }
        }
        return bytes;
      }

      ObservedBytes.prototype = OriginalBytes.prototype;
      Object.defineProperty(ObservedBytes, Symbol.species, { value: ObservedBytes });
      globalThis.Uint8Array = ObservedBytes;
      Object.defineProperty(OriginalBytes.prototype, 'constructor', {
        ...constructor,
        value: ObservedBytes
      });
      try {
        g = fixture();
        g.state.ready = true;

        g.protocol.handle('app', () => new Response(new OriginalBytes([1, 2, 3])));

        if (mode === 'HEAD') {
          armed = true;
          await rejects(
            g.api.fetch('app://bundle/a', { method: 'HEAD' }),
            'ERR_FIREFLY_PROTOCOL_CANCELED'
          );
        } else {
          const response = await g.api.fetch('app://bundle/a');
          armed = true;
          await rejects(response.body.getReader().read(), 'ERR_FIREFLY_PROTOCOL_CANCELED');
        }
        check(revoked, 'Actual byte-result reentry reached ' + mode + '/' + teardown);
      } finally {
        armed = false;
        globalThis.Uint8Array = OriginalBytes;
        Object.defineProperty(OriginalBytes.prototype, 'constructor', constructor);
        g?.api.stop();
      }
    }
  }
  console.log('PASS main fetch pre-abort body cancellation signal reason and reader disposal');
}

{
  const f = fixture();
  f.state.ready = true;

  f.state.onRegister = (name) => f.protocol.unhandle(name);

  fails(() => f.protocol.handle('app', () => new Response()), 'ERR_FIREFLY_PROTOCOL_CANCELED');

  check(
    !f.state.routes.has('app') && !f.protocol.isProtocolHandled('app'),
    'Late admission cannot restore revoked route'
  );
  f.state.onRegister = undefined;
  let signal;

  f.protocol.handle('app', (request) => {
    signal = request.signal;
    return new Promise(() => {});
  });

  const pending = f.api.fetch('app://bundle/a');
  const rejection = rejects(pending, 'ERR_FIREFLY_PROTOCOL_CANCELED');
  await tick();
  f.api.stop();
  f.api.stop();
  await rejection;
  check(signal.aborted && !f.state.routes.size, 'Stop retires pending handler and native routes');

  fails(() => f.protocol.handle('app', () => new Response()), 'ERR_FIREFLY_APP_EXITED');

  console.log(
    'PASS reentrant registration revocation pending fetch shutdown and permanent ownership stop'
  );
}
console.log('DONE modern protocol handlers');
