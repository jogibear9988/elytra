/** Blob/File/FormData comparisons against actual Node, independent of wire boundaries. */
import { Blob as BufferBlob, File as BufferFile } from 'node:buffer';

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

async function rejects(promise, Type = TypeError) {
  try {
    await promise;
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

await run('global and node:buffer identities', () => {
  check(Blob === BufferBlob && File === BufferFile, 'shared buffer exports');
  check(new File([], 'x') instanceof Blob && !(new Blob() instanceof File), 'File inheritance');
  check(
    Object.getOwnPropertyDescriptor(Blob.prototype, 'size').get.call(new Blob(['x'])) === 1,
    'public Blob getter'
  );
  check(
    Object.prototype.toString.call(new Blob()) === '[object Blob]' &&
      Object.prototype.toString.call(new File([], 'x')) === '[object File]',
    'tags'
  );
  for (const name of ['Blob', 'File', 'FormData']) {
    const descriptor = Object.getOwnPropertyDescriptor(globalThis, name);
    check(
      descriptor.writable && descriptor.configurable && !descriptor.enumerable,
      'global descriptors'
    );
  }
});

await run('Blob mixed parts immutable snapshots and result ownership', async () => {
  const input = new Uint8Array([0, 65, 66, 0]);
  const blob = new Blob(['x', new DataView(input.buffer, 1, 2), new Blob(['y'])], {
    type: 'TEXT/PLAIN'
  });
  input.fill(9);
  check(
    blob.size === 4 && blob.type === 'text/plain' && (await blob.text()) === 'xABy',
    'parts and MIME'
  );
  const bytes = await blob.bytes();
  bytes[0] = 0;
  check(
    (await blob.text()) === 'xABy' && (await blob.arrayBuffer()).byteLength === 4,
    'independent outputs'
  );
  check(new Blob([], { type: 'bad\u0001type' }).type === '', 'invalid MIME chars');
});

await run('Blob scalar strings BOM and newline modes', async () => {
  check((await new Blob(['\uFEFFa']).bytes()).join() === '239,187,191,97', 'BOM is content');
  check((await new Blob(['\uFEFFa']).text()) === 'a', 'text strips BOM');
  check((await new Blob(['\uD800']).text()) === '\uFFFD', 'scalar replacement');
  const text = 'a\rb\r\nc\nd';
  const native = process.platform === 'win32' ? 'a\rb\r\nc\r\nd' : 'a\rb\nc\nd';
  check((await new Blob([text], { endings: 'native' }).text()) === native, 'native endings');
  check((await new Blob([text]).text()) === text, 'transparent endings');

  throws(() => new Blob([], { endings: 'wrong' }));

  throws(() => new Blob('not a sequence object'));

  throws(() => new Blob([Symbol('invalid')]));
});

await run('Blob slice ranges and fractional indices', async () => {
  const blob = new Blob(['abcdef']);
  check((await blob.slice(1.9, 4.8).text()) === 'bcd', 'integer indices');
  check(
    (await blob.slice(-3).text()) === 'def' && (await blob.slice(4, 2).text()) === '',
    'relative ranges'
  );
  check((await blob.slice(NaN, Infinity).text()) === '', 'Node nonfinite bounds');
  const file = new File(['file'], 'file.txt');
  const sliced = file.slice(1, 3, 'TEXT/PLAIN');
  check(
    sliced.constructor === Blob &&
      !(sliced instanceof File) &&
      sliced.type === 'text/plain' &&
      (await sliced.text()) === 'il',
    'slice returns Blob'
  );
});

await run('Blob streams BYOB EOF repeatability and cancellation', async () => {
  const blob = new Blob([new Uint8Array(150001).fill(97)]);
  const reader = blob.stream().getReader({ mode: 'byob' });
  let count = 0;
  while (true) {
    const input = new Uint8Array(997);
    const { value, done } = await reader.read(input);
    check(input.byteLength === 0, 'BYOB transfer');
    if (done) {
      break;
    }
    check(
      value.every((byte) => byte === 97),
      'stream bytes'
    );
    count += value.byteLength;
  }
  check(count === blob.size, 'all streamed bytes');
  check(
    (await new Blob().stream().getReader({ mode: 'byob' }).read(new Uint8Array(4))).done,
    'empty EOF'
  );
  const other = blob.stream().getReader();
  await other.read();
  await other.cancel();
  check(
    (await other.read()).done && (await blob.bytes()).length === 150001,
    'stream cancellation preserves Blob'
  );
});

await run('File names timestamps and receiver checks', async () => {
  const file = new File(['file'], '\uFEFF\uD800.txt', { type: 'TEXT/PLAIN', lastModified: -2.9 });
  check(
    file.name === '\uFEFF\uFFFD.txt' && file.lastModified === -2.9 && file.type === 'text/plain',
    'File metadata'
  );
  check((await file.text()) === 'file', 'File content');
  check(new File([], 'x', { lastModified: NaN }).lastModified === 0, 'NaN timestamp');

  throws(() => new File([]));

  throws(() => Object.getOwnPropertyDescriptor(File.prototype, 'name').get.call({}));
});

await run('FormData ordered duplicates set delete and callbacks', () => {
  const form = new FormData();
  form.append('a', 'first');
  form.append('b', 'other');
  form.append('a', 'last');
  check(
    form.getAll('a').join() === 'first,last' &&
      form.get('missing') === null &&
      !form.has('missing'),
    'duplicate values'
  );
  form.set('a', 'new');
  check(JSON.stringify([...form]) === '[["a","new"],["b","other"]]', 'set ordering');
  form.delete('b');
  const seen = [];

  form.forEach(function (value, name, owner) {
    check(this === seen && owner === form, 'callback');
    this.push(name + ':' + value);
  }, seen);

  check(
    seen.join() === 'a:new' &&
      [...form.keys()].join() === 'a' &&
      [...form.values()].join() === 'new',
    'iterators'
  );
});

await run('FormData scalar conversion File normalization and validation', async () => {
  const form = new FormData();
  form.append('\uD800', '\uFEFF\uD800');
  check(form.get('\uFFFD') === '\uFEFF\uFFFD', 'scalar conversion');
  const file = new File(['data'], 'original.txt', { type: 'text/plain', lastModified: 123 });
  form.append('file', file);
  check(form.get('file') === file, 'existing File identity');
  form.append('renamed', file, 'new.txt');
  check(
    form.get('renamed').name === 'new.txt' && form.get('renamed').lastModified === 123,
    'rename timestamp'
  );
  form.append('blob', new Blob(['blob']));
  check(form.get('blob') instanceof File && form.get('blob').name === 'blob', 'default filename');

  throws(() => form.append('wrong', 'text', 'filename'));

  throws(() => form.append('missing'));

  throws(() => FormData.prototype.get.call({}, 'x'));
});

await run('Blob bodies clones and Body.blob MIME conversion', async () => {
  const blob = new Blob(['blob body'], { type: 'text/plain' });
  const response = new Response(blob);
  check(response.headers.get('content-type') === 'text/plain', 'Blob content type');
  const clone = response.clone();
  check(
    (await response.text()) === 'blob body' && (await clone.text()) === 'blob body',
    'Blob body tee'
  );
  const consumed = await new Response('text', {
    headers: { 'content-type': 'Text/Plain; CHARSET="UTF-8"' }
  }).blob();
  check(
    consumed instanceof Blob &&
      consumed.type === 'text/plain;charset=utf-8' &&
      (await consumed.text()) === 'text',
    'MIME serialization'
  );
  check(
    (await new Response('x', { headers: { 'content-type': 'not a MIME' } }).blob()).type === '',
    'invalid MIME'
  );
});

await run('URL encoded form consumer repeated fields malformed escapes and BOM', async () => {
  const response = new Response('a=one&a=two&space=a+b&bad=%FF', {
    headers: { 'content-type': 'application/x-www-form-urlencoded;charset=latin1' }
  });
  const pending = response.formData();
  check(response.bodyUsed, 'consumer disturbs immediately');
  const form = await pending;
  check(
    form.getAll('a').join() === 'one,two' &&
      form.get('space') === 'a b' &&
      form.get('bad') === '\uFFFD',
    'form decode'
  );
  const bom = await new Response(new Uint8Array([239, 187, 191, 120, 61, 49]), {
    headers: { 'content-type': 'application/x-www-form-urlencoded' }
  }).formData();
  check(bom.get('\uFEFFx') === '1', 'BOM preserved in name');
  await rejects(response.formData());
});

await run('Multipart encoding snapshot and FormData roundtrip', async () => {
  const form = new FormData();
  form.append('field', 'one\ntwo');
  form.append(
    'file',
    new File([new Uint8Array([0, 1, 255])], 'dir/é.txt', { type: 'application/octet-stream' })
  );
  form.append('field', 'last');
  const request = new Request('http://example.com', { method: 'POST', body: form });
  form.set('field', 'changed later');
  check(
    request.headers.get('content-type').startsWith('multipart/form-data; boundary='),
    'boundary header'
  );
  const parsed = await request.formData();
  check(parsed.getAll('field').join('|') === 'one\r\ntwo|last', 'multipart normalized snapshot');
  const file = parsed.get('file');
  check(
    file instanceof File &&
      file.name === 'dir/é.txt' &&
      file.type === 'application/octet-stream' &&
      (await file.bytes()).join() === '0,1,255',
    'multipart File'
  );
  check([...parsed.keys()].join() === 'field,file,field', 'wire entry order');
});

await run('Multipart incoming base64 and malformed bodies', async () => {
  const body =
    '--x\r\nContent-Disposition: form-data; name="file"; filename="x.txt"\r\nContent-Transfer-Encoding: base64\r\nContent-Type: text/plain\r\n\r\naGVsbG8=\r\n--x--\r\n';
  const parsed = await new Response(body, {
    headers: { 'content-type': 'multipart/form-data; boundary="x"' }
  }).formData();
  check((await parsed.get('file').text()) === 'hello', 'base64 transfer decode');
  for (const [body, type] of [
    ['body', 'text/plain'],
    ['', 'multipart/form-data'],
    ['--x\r\n', 'multipart/form-data; boundary=x']
  ]) {
    const response = new Response(body, { headers: { 'content-type': type } });
    await rejects(response.formData());
    check(response.bodyUsed, 'failed form parse consumes body');
  }
});

await run('byte APIs capture original stream and codec constructors', async () => {
  const saved = [globalThis.ReadableStream, globalThis.TextEncoder, globalThis.TextDecoder];
  try {
    globalThis.ReadableStream = null;
    globalThis.TextEncoder = null;
    globalThis.TextDecoder = null;
    const blob = new Blob(['captured']).slice(1, 4);
    check(
      (await blob.text()) === 'apt' && (await blob.bytes()).join() === '97,112,116',
      'internal slice readers'
    );
    check((await (await new Response(blob).blob()).text()) === 'apt', 'captured Body codecs');
  } finally {
    [globalThis.ReadableStream, globalThis.TextEncoder, globalThis.TextDecoder] = saved;
  }
});
console.log('DONE Fetch data');
