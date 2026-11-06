/** Compare observable Fetch headers, body ownership and object metadata with Node. */
function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function throws(action, Type = TypeError) {
  try {
    action();
  } catch (error) {
    check(error instanceof Type, 'wrong exception');
    return;
  }
  throw new Error('missing exception');
}

async function rejects(action, Type = TypeError) {
  try {
    await action;
  } catch (error) {
    check(error instanceof Type, 'wrong rejection');
    return;
  }
  throw new Error('missing rejection');
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

await run('header records normalization and sorted iteration', () => {
  const headers = new Headers({ Z: ' z ', A: 'one' });
  headers.append('a', 'two');
  check(headers.get('A') === 'one, two' && headers.has('z'), 'combined values');
  check(JSON.stringify([...headers]) === '[["a","one, two"],["z","z"]]', 'sorted normalized');
  headers.set('a', 'new');
  headers.delete('Z');
  check(
    headers.get('a') === 'new' && !headers.has('z') && headers.get('missing') === null,
    'mutations'
  );
  const seen = [];

  headers.forEach(function (value, key, owner) {
    check(owner === headers && this === seen, 'callback receiver');
    this.push(key + ':' + value);
  }, seen);

  check(seen.join() === 'a:new', 'forEach');
  check([...headers.keys()].join() === 'a' && [...headers.values()].join() === 'new', 'iterators');
});

await run('set-cookie preservation and constructor copies', () => {
  const original = new Headers([
    ['set-cookie', 'a=1'],
    ['set-cookie', 'b=2']
  ]);
  check(original.getSetCookie().join('|') === 'a=1|b=2', 'cookie entries');
  check(
    [...original].length === 2 && original.get('set-cookie') === 'a=1, b=2',
    'cookie iteration'
  );
  const copy = new Headers(original);
  original.set('set-cookie', 'changed');
  check(copy.getSetCookie().join('|') === 'a=1|b=2', 'independent header list');
  const cookies = new Headers({ cookie: 'a=1' });
  cookies.append('Cookie', 'b=2');
  check(
    cookies.get('cookie') === 'a=1; b=2' && [...cookies][0][1] === 'a=1; b=2',
    'request cookie separator'
  );
});

await run('header validation and receivers', () => {
  for (const action of [
    () => new Headers({ 'bad name': 'x' }),
    () => new Headers({ x: 'bad\nvalue' }),
    () => new Headers({ x: '\0' }),
    () => new Headers({ x: '€' }),
    () => new Headers(['ab']),
    () => new Headers([['x']]),
    () => Headers.prototype.get.call({}, 'x'),
    () => new Headers().append('x')
  ]) {
    throws(action);
  }
  const trimmed = new Headers({ x: '\r\n ok \r\n' });
  check(trimmed.get('x') === 'ok', 'HTTP edge whitespace');
});

await run('response defaults text and null body reuse', async () => {
  const response = new Response('hello');
  check(
    response.status === 200 &&
      response.ok &&
      response.type === 'default' &&
      response.url === '' &&
      !response.redirected,
    'default response'
  );
  check(
    response.headers.get('content-type') === 'text/plain;charset=UTF-8' && !response.bodyUsed,
    'default type'
  );
  check((await response.text()) === 'hello' && response.bodyUsed, 'consume once');
  await rejects(response.text());
  const empty = new Response();
  check(
    empty.body === null &&
      (await empty.text()) === '' &&
      (await empty.text()) === '' &&
      !empty.bodyUsed,
    'null body'
  );
  check((await new Response('').text()) === '', 'zero-length byte stream');
});

await run('body snapshots views bytes and scalar replacement', async () => {
  const input = new Uint8Array([0, 1, 2, 3]);
  const response = new Response(new DataView(input.buffer, 1, 2));
  input.fill(9);
  check((await response.bytes()).join() === '1,2', 'byte snapshot boundaries');
  const buffer = await new Response(new Uint8Array([4, 5])).arrayBuffer();
  check(buffer.byteLength === 2 && new Uint8Array(buffer).join() === '4,5', 'owned buffer');
  check((await new Response('\uD800').text()) === '\uFFFD', 'scalar replacement');
  check((await new Response(new Uint8Array([239, 187, 191, 97])).text()) === 'a', 'UTF8 BOM');
});

await run('response JSON and URLSearchParams bodies', async () => {
  const response = Response.json({ answer: 42 }, { status: 201, headers: { x: 'yes' } });
  check(
    response.headers.get('content-type') === 'application/json' &&
      response.status === 201 &&
      (await response.json()).answer === 42,
    'JSON response'
  );

  throws(() => Response.json(undefined));

  await rejects(new Response('{').json(), SyntaxError);
  const form = new Response(new URLSearchParams({ x: 'a b' }));
  check(
    form.headers.get('content-type') === 'application/x-www-form-urlencoded;charset=UTF-8' &&
      (await form.text()) === 'x=a+b',
    'urlencoded'
  );
});

await run('response statuses and static response guards', async () => {
  for (const status of [199, 600, NaN]) {
    throws(() => new Response(null, { status }), RangeError);
  }
  for (const status of [204, 205, 304]) {
    throws(() => new Response('', { status }));
  }

  throws(() => new Response(null, { statusText: 'bad\ntext' }));

  check(new Response(null, { status: 200.9 }).status === 200, 'status integer conversion');
  const error = Response.error();
  check(
    error.status === 0 && error.type === 'error' && !error.ok && error.body === null,
    'error response'
  );

  throws(() => error.headers.set('x', 'y'));

  const redirect = Response.redirect('https://example.com/a', 307);
  check(
    redirect.status === 307 && redirect.headers.get('location') === 'https://example.com/a',
    'redirect response'
  );

  throws(() => redirect.headers.append('x', 'y'));

  throws(() => redirect.clone().headers.set('x', 'y'));
});

await run('body cloning locking direct reads and cancellation', async () => {
  const response = new Response('cloned');
  const copy = response.clone();
  check(!response.bodyUsed && !copy.bodyUsed, 'cloning does not disturb');
  check(
    (await Promise.all([response.text(), copy.text()])).join() === 'cloned,cloned',
    'tee bodies'
  );
  const direct = new Response('read');
  const reader = direct.body.getReader();
  check(!direct.bodyUsed, 'lock alone does not disturb');

  throws(() => direct.clone());

  await reader.read();
  check(direct.bodyUsed, 'direct read is observable');

  throws(() => direct.clone());

  await reader.cancel();
  reader.releaseLock();
  const cancelled = new Response('cancel');
  await cancelled.body.cancel();
  check(cancelled.bodyUsed, 'cancel disturbs');
  await rejects(cancelled.text());
});

await run('stream body errors and invalid chunks', async () => {
  const reason = new Error('body failed');
  const response = new Response(
    new ReadableStream({
      start(controller) {
        controller.error(reason);
      }
    })
  );
  try {
    await response.text();
    throw new Error('missing failure');
  } catch (error) {
    check(error === reason, 'body error identity');
  }
  await rejects(new Response(ReadableStream.from(['not bytes'])).text());
});

await run('request URL method headers and metadata', async () => {
  const request = new Request('http://example.com/a#fragment', {
    method: 'post',
    body: 'body',
    headers: { x: 'yes' }
  });
  check(
    request.url === 'http://example.com/a#fragment' && request.method === 'POST',
    'request URL/method'
  );
  check(
    request.headers.get('x') === 'yes' &&
      request.headers.get('content-type') === 'text/plain;charset=UTF-8',
    'request headers'
  );
  check(
    request.redirect === 'follow' &&
      request.mode === 'cors' &&
      request.credentials === 'same-origin' &&
      request.cache === 'default' &&
      request.duplex === 'half',
    'request modes'
  );
  check(
    request.referrer === 'about:client' &&
      request.destination === '' &&
      !request.keepalive &&
      request.signal instanceof AbortSignal,
    'request metadata'
  );
  check((await request.text()) === 'body', 'request consumption');
});

await run('request validation streaming bodies and cloning', async () => {
  for (const action of [
    () => new Request('/relative'),
    () => new Request('http://user:pass@example.com'),
    () => new Request('http://example.com', { body: 'x' }),
    () => new Request('http://example.com', { method: 'TRACE' }),
    () => new Request('http://example.com', { method: 'POST', body: new ReadableStream() })
  ]) {
    throws(action);
  }
  const request = new Request('http://example.com', {
    method: 'POST',
    body: ReadableStream.from([new Uint8Array([97])]),
    duplex: 'half'
  });
  const copy = request.clone();
  check((await Promise.all([request.text(), copy.text()])).join() === 'a,a', 'request cloning');

  throws(() => request.clone());
});

await run('request transfer and independent signal following', async () => {
  const control = new AbortController();
  const original = new Request('http://example.com', {
    method: 'POST',
    body: 'transfer',
    signal: control.signal
  });
  const next = new Request(original);
  check(
    original.bodyUsed &&
      original.body.locked &&
      !next.bodyUsed &&
      (await next.text()) === 'transfer',
    'transferred ownership'
  );
  check(
    original.signal !== control.signal && next.signal !== original.signal,
    'new signal identities'
  );
  const reason = new Error('stop');
  control.abort(reason);
  check(original.signal.reason === reason && next.signal.reason === reason, 'signal reason');
});

console.log('DONE Fetch objects');
