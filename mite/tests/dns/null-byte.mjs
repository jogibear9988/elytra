/** NUL validation must never schedule a prefix lookup or evaluate later options. */
import assert from 'node:assert/strict';
import dns from 'node:dns';

if (process.argv.includes('--node-24.15-reference')) {
  assert.equal(process.versions.node, '24.15.0');
  // Exact audited old-reference bug only; this is never accepted from Mite.
  assert.deepEqual(await dns.promises.lookup('127.0.0.1\0ignored'), {
    address: '127.0.0.1',
    family: 4
  });
  console.log('PASS exact Node24.15 embedded-NUL prefix reference');
} else {
  const expected = { name: 'TypeError', code: 'ERR_INVALID_ARG_VALUE' };
  let callbacks = 0;
  let optionReads = 0;
  const options = {
    get family() {
      optionReads++;
      throw Error('NUL validation must precede option access');
    }
  };
  for (const hostname of ['127.0.0.1\0ignored', 'localhost\0ignored', '\0']) {
    assert.throws(() => dns.lookup(hostname, options, () => callbacks++), expected);

    assert.throws(() => dns.lookup(hostname), expected);
  }
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(callbacks, 0);
  assert.equal(optionReads, 0);
  console.log('PASS DNS callback NUL rejects before callback/options/native lookup');

  for (const hostname of ['127.0.0.1\0ignored', 'localhost\0ignored', '\0']) {
    assert.throws(() => dns.promises.lookup(hostname, options), expected);
  }
  assert.equal(optionReads, 0);
  console.log('PASS DNS Promise NUL rejects synchronously before option access');

  let synchronous = true;
  await new Promise((resolve, reject) => {
    dns.lookup('127.0.0.1', (error, address, family) => {
      try {
        assert.equal(synchronous, false);
        assert.equal(error, null);
        assert.equal(address, '127.0.0.1');
        assert.equal(family, 4);
        resolve();
      } catch (failure) {
        reject(failure);
      }
    });

    synchronous = false;
  });
  if (process.versions.mite) {
    // This slice changes only NUL admission. Preserve Mite's pre-existing
    // Promise rejection for other invalid options rather than changing timing.
    const invalid = dns.promises.lookup('127.0.0.1', { family: 'invalid' });
    assert.equal(typeof invalid.then, 'function');
    await assert.rejects(invalid, { code: 'ERR_INVALID_ARG_VALUE' });
  } else {
    assert.throws(() => dns.promises.lookup('127.0.0.1', { family: 'invalid' }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  }
  console.log('PASS DNS valid callback and non-NUL validation retain documented scheduling');
}
