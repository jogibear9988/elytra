/** Fetch against independent peers; response barriers distinguish streaming from buffering. */
import { setTimeout as sleep } from 'node:timers/promises';

const base = `http://127.0.0.1:${process.argv[2]}`;
const other = `http://127.0.0.1:${process.argv[3]}`;

const encode = (text) => new TextEncoder().encode(text);

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function failure(promise, expected) {
  try {
    await promise;
  } catch (error) {
    check(typeof expected === 'function' ? expected(error) : error === expected, 'wrong rejection');
    return;
  }
  throw new Error('missing failure');
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

async function release(id) {
  await (await fetch(`${base}/release/${id}`)).text();
}

await run('GET URL query immutable headers and metadata', async () => {
  const response = await fetch(base + '/json?x=1#fragment', { headers: { host: 'wrong.example' } });
  check(
    response instanceof Response &&
      response.status === 200 &&
      response.statusText === 'OK' &&
      response.type === 'basic',
    'response metadata'
  );
  check(
    response.url === base + '/json?x=1' && !response.redirected && !response.bodyUsed,
    'response URL/state'
  );
  let immutable = false;
  try {
    response.headers.set('x', 'y');
  } catch (error) {
    immutable = error instanceof TypeError;
  }
  check(immutable && response.headers.get('x-peer') === 'independent', 'network header guard');
  const result = await response.json();
  check(
    result.path === '/json?x=1' &&
      result.host === `127.0.0.1:${process.argv[2]}` &&
      response.bodyUsed,
    'JSON body and URL-derived Host'
  );
});

await run('HTTP failure status resolves normally', async () => {
  const response = await fetch(base + '/error');
  check(
    response.status === 418 && !response.ok && (await response.text()) === 'teapot',
    'HTTP failure is a response'
  );
});

await run('POST text UTF8 length and URLSearchParams', async () => {
  const response = await fetch(base + '/echo', { method: 'POST', body: 'héllo' });
  const reply = await response.json();
  check(
    reply.method === 'POST' &&
      reply.body === 'héllo' &&
      reply.length === 6 &&
      reply.type === 'text/plain;charset=UTF-8',
    'text upload'
  );
  const form = await (
    await fetch(base + '/echo', { method: 'POST', body: new URLSearchParams({ x: 'a b' }) })
  ).json();
  check(
    form.body === 'x=a+b' && form.type === 'application/x-www-form-urlencoded;charset=UTF-8',
    'form upload'
  );
});

await run('streamed upload and existing Request ownership', async () => {
  let chunks = 0;
  const request = new Request(base + '/echo', {
    method: 'POST',
    duplex: 'half',
    body: new ReadableStream({
      pull(controller) {
        if (chunks++ < 16) {
          controller.enqueue(new Uint8Array(16384).fill(97));
        } else {
          controller.close();
        }
      }
    })
  });
  const result = await (await fetch(request)).json();
  check(
    request.bodyUsed && result.length === 262144 && result.body === 'a'.repeat(262144),
    'stream upload'
  );
});

await run('HEAD empty statuses and separate Set-Cookie', async () => {
  for (const [path, options, status] of [
    ['/json', { method: 'HEAD' }, 200],
    ['/empty', {}, 204],
    ['/not-modified', {}, 304]
  ]) {
    const response = await fetch(base + path, options);
    check(
      response.status === status &&
        response.body === null &&
        (await response.text()) === '' &&
        !response.bodyUsed,
      'null response body'
    );
  }
  const response = await fetch(base + '/cookies');
  check(response.headers.getSetCookie().join('|') === 'a=1|b=2', 'response cookies');
  await response.text();
});

await run('redirect follow manual error and method rewriting', async () => {
  const redirect = '/redirect/302?to=' + encodeURIComponent('/echo');
  const response = await fetch(base + redirect, { method: 'POST', body: 'payload' });
  const reply = await response.json();
  check(
    response.redirected &&
      response.url === base + '/echo' &&
      reply.method === 'GET' &&
      reply.body === '' &&
      reply.type === null,
    '302 rewrites POST'
  );
  const manual = await fetch(base + redirect, { redirect: 'manual' });
  check(
    manual.status === 302 && !manual.redirected && manual.headers.get('location') === '/echo',
    'manual redirect'
  );
  await manual.text();
  await failure(
    fetch(base + redirect, { redirect: 'error' }),
    (error) => error instanceof TypeError
  );
});

await run('307 replay and cross-origin credential removal', async () => {
  const response = await fetch(base + '/redirect/307?to=%2Fecho', {
    method: 'POST',
    body: 'replay'
  });
  const reply = await response.json();
  check(reply.method === 'POST' && reply.body === 'replay', 'replayable body');
  const cross = await (
    await fetch(base + '/redirect/302?to=' + encodeURIComponent(other + '/echo'), {
      headers: { authorization: 'secret', cookie: 'private=1' }
    })
  ).json();
  check(cross.authorization === null && cross.cookie === null, 'cross-origin secrets removed');
});

await run('streaming response BYOB before peer release', async () => {
  const response = await fetch(base + '/body/incremental');
  const reader = response.body.getReader({ mode: 'byob' });
  const first = await reader.read(new Uint8Array(1024));
  check(
    !first.done && first.value.length > 0 && first.value.every((byte) => byte === 97),
    'BYOB first bytes'
  );
  let count = first.value.byteLength;
  await release('incremental');
  while (true) {
    const { value, done } = await reader.read(new Uint8Array(16384));
    if (done) {
      break;
    }
    check(
      value.every((byte) => byte === 97),
      'response bytes'
    );
    count += value.byteLength;
  }
  check(count === 1048576 && response.bodyUsed, 'incremental response complete');
});

await run('network response clone byte ownership', async () => {
  const response = await fetch(base + '/json');
  const clone = response.clone();
  const values = await Promise.all([response.text(), clone.text()]);
  check(values[0] === values[1] && response.bodyUsed && clone.bodyUsed, 'network tee');
});

await run('pre-abort skips request and live header abort', async () => {
  const reason = new Error('cancel request');
  await failure(fetch(base + '/never', { signal: AbortSignal.abort(reason) }), reason);
  let cancelled;
  const body = new ReadableStream({
    cancel(value) {
      cancelled = value;
    }
  });
  await failure(
    fetch(base + '/never', {
      method: 'POST',
      body,
      duplex: 'half',
      signal: AbortSignal.abort(reason)
    }),
    reason
  );
  check(cancelled === reason && !body.locked, 'pre-abort cancels upload source');
  const control = new AbortController();
  const pending = fetch(base + '/headers/pending', { signal: control.signal });
  // Wait until an independent control connection confirms the held request exists.
  let seen = false;
  for (let index = 0; index < 40 && !seen; index++) {
    seen = (await (await fetch(base + '/seen/pending')).json()).seen;
    if (!seen) {
      await sleep(5);
    }
  }
  check(seen, 'held request reached server');
  control.abort(reason);
  await failure(pending, reason);
  await release('pending');
});

await run('abort during response consumption preserves reason', async () => {
  const control = new AbortController();
  const response = await fetch(base + '/body/aborted', { signal: control.signal });
  const consuming = response.text();
  const reason = new Error('cancel body');
  control.abort(reason);
  await failure(consuming, reason);
  await release('aborted');
});

await run('reader cancellation closes native peer', async () => {
  const response = await fetch(base + '/body/cancelled');
  const reader = response.body.getReader();
  check(!(await reader.read()).done, 'initial body bytes');
  await reader.cancel(new Error('stop reading'));
  await release('cancelled');
});

await run('truncation redirect limits and unreplayable body failures', async () => {
  await failure((await fetch(base + '/truncated')).text(), (error) => error instanceof TypeError);
  await failure(fetch(base + '/loop'), (error) => error instanceof TypeError);
  await failure(
    fetch(base + '/redirect/307?to=%2Fecho', {
      method: 'POST',
      duplex: 'half',
      body: ReadableStream.from([encode('stream')])
    }),
    (error) => error instanceof TypeError
  );
});

console.log('DONE Fetch network');
