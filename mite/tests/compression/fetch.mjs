/** Compressed HTTP bodies keep encoded headers while exposing incremental decoded bytes. */
const base = `http://127.0.0.1:${process.argv[2]}`;
const expected = 1048577;

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

await run('gzip deflate raw fallback stacked and concatenated members', async () => {
  for (const name of ['gzip', 'deflate', 'raw', 'stack', 'concat']) {
    const response = await fetch(base + '/' + name);
    const bytes = await response.bytes();
    check(
      bytes.length === expected && bytes.every((value) => value === 97),
      'decoded body ' + name
    );
    check(
      response.headers.get('content-encoding') !== null &&
        Number(response.headers.get('content-length')) < bytes.length,
      'encoded metadata ' + name
    );
  }
});

await run('compressed delivery before independent peer release', async () => {
  const response = await fetch(base + '/partial');
  const reader = response.body.getReader({ mode: 'byob' });
  let count = (await reader.read(new Uint8Array(1024))).value.byteLength;
  check(count > 0 && count <= 4096, 'early decompressed bytes');
  await (await fetch(base + '/release/partial')).text();
  while (true) {
    const { value, done } = await reader.read(new Uint8Array(8192));
    if (done) {
      break;
    }
    count += value.byteLength;
  }
  check(count === expected, 'incremental decoded length');
});

await run('compressed cancel and live abort close peers', async () => {
  for (const name of ['cancel', 'abort']) {
    const control = new AbortController();
    const response = await fetch(base + '/' + name, { signal: control.signal });
    const reader = response.body.getReader();
    check(!(await reader.read()).done, 'first decoded bytes');
    if (name === 'cancel') {
      await reader.cancel(new Error('stop inflate'));
    } else {
      const reason = new Error('abort inflate');
      control.abort(reason);
      try {
        await reader.read();
        throw new Error('missing abort');
      } catch (error) {
        check(error === reason, 'abort reason');
      }
    }
    await (await fetch(base + '/release/' + name)).text();
  }
});

await run('checksum rejects and empty framed gzip succeeds', async () => {
  let failed = false;
  try {
    await (await fetch(base + '/badcrc')).bytes();
  } catch (error) {
    failed = error instanceof TypeError && error.cause?.code === 'Z_DATA_ERROR';
  }
  check(failed, 'CRC verification');
  check((await (await fetch(base + '/empty')).text()) === '', 'empty encoded frame');
});
console.log('DONE compressed Fetch');
