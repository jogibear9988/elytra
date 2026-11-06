/** Multipart and immutable Blob/File payloads against an independent MIME/HTTP peer. */
import { createHash } from 'node:crypto';
const base = `http://127.0.0.1:${process.argv[2]}`;

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

await run('Blob File and empty uploads preserve lengths and MIME', async () => {
  for (const body of [
    new Blob(['héllo'], { type: 'TEXT/PLAIN' }),
    new File(['file'], 'x.txt', { type: 'text/plain' }),
    new Blob()
  ]) {
    const reply = await (await fetch(base + '/blob', { method: 'POST', body })).json();
    check(reply.length === body.size && reply.declared === body.size, 'byte length');
    check(
      reply.type === (body.type || null) &&
        reply.hex === Buffer.from(await body.arrayBuffer()).toString('hex'),
      'payload and MIME'
    );
  }
});

await run('multipart upload duplicate fields and binary File', async () => {
  const bytes = new Uint8Array(262145).fill(97);
  const hash = createHash('sha256').update(bytes).digest('hex');
  const form = new FormData();
  form.append('field', 'one\ntwo');
  form.append('file', new File([bytes], 'diagram.bin', { type: 'application/octet-stream' }));
  form.append('field', 'last');
  const pending = fetch(base + '/form', { method: 'POST', body: form });
  form.set('field', 'changed after extraction');
  const reply = await (await pending).json();
  check(reply.validLength && reply.parts.length === 3, 'multipart framing');
  check(
    reply.parts[0].name === 'field' &&
      reply.parts[0].text === 'one\r\ntwo' &&
      reply.parts[2].text === 'last',
    'field snapshots'
  );
  check(
    reply.parts[1].filename === 'diagram.bin' &&
      reply.parts[1].size === bytes.length &&
      reply.parts[1].sha256 === hash,
    'binary File integrity'
  );
});

await run('307 Blob replay', async () => {
  const blob = await (
    await fetch(base + '/redirect-blob', { method: 'POST', body: new Blob(['replay']) })
  ).json();
  check(blob.hex === '7265706c6179' && blob.declared === 6, 'Blob replay');
});

await run('HTTP formData response large field base64 File and ordering', async () => {
  const response = await fetch(base + '/response-form');
  const form = await response.formData();
  check(
    response.bodyUsed && [...form.keys()].join() === 'field,file,field',
    'response entry order'
  );
  check(
    form.getAll('field')[0].length === 1100000 &&
      form.getAll('field')[0] === 'a'.repeat(1100000) &&
      form.getAll('field')[1] === 'last',
    'no field truncation'
  );
  const file = form.get('file');
  check(
    file instanceof File &&
      file.name === 'dir/é.txt' &&
      file.type === 'text/plain' &&
      (await file.text()) === 'hello',
    'response File'
  );
});

await run('HTTP Blob consumer and URL form data', async () => {
  const response = await fetch(base + '/response-blob');
  const blob = await response.blob();
  check(
    blob instanceof Blob &&
      blob.type === 'application/octet-stream' &&
      (await blob.bytes()).join() === '0,1,255',
    'network Blob'
  );
  const form = await (await fetch(base + '/response-urlencoded')).formData();
  check(
    form.getAll('field').join() === 'one,two' && form.get('space') === 'a b',
    'network urlencoded'
  );
});

console.log('DONE Fetch data network');
