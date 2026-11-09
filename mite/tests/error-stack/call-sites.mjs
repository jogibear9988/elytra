/** CallSites expose actual capture-time locations, with non-forgeable native receivers. */
import assert from 'node:assert/strict';
import { stackCaptureTop } from './helpers.mjs';

const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');
try {
  Error.prepareStackTrace = (_target, sites) => sites;

  const sites = stackCaptureTop({}).stack;
  assert.ok(Array.isArray(sites) && sites.length >= 3);
  for (const [index, name] of [
    'stackCaptureLeaf',
    'stackCaptureMiddle',
    'stackCaptureTop'
  ].entries()) {
    const site = sites[index];
    assert.equal(site.getFunctionName(), name);
    assert.ok(/(?:^|\/)helpers\.mjs$/.test(site.getFileName()));
    assert.equal(site.getScriptNameOrSourceURL(), site.getFileName());
    assert.ok(Number.isInteger(site.getLineNumber()) && site.getLineNumber() > 0);
    assert.ok(Number.isInteger(site.getColumnNumber()) && site.getColumnNumber() > 0);
    if (process.versions.mite) {
      assert.throws(
        () => site.isConstructor(),
        (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
      );
    } else {
      assert.equal(site.isConstructor(), false);
    }
    assert.equal(site.isNative(), false);
    assert.equal(typeof site.toString(), 'string');
    assert.ok(site.toString().includes(name));
    assert.ok(site.toString().includes('helpers.mjs'));
  }
  assert.equal(sites[0].getFunctionName.call(sites[1]), 'stackCaptureMiddle');
  assert.equal(sites[0].getFileName.call(sites[1]), sites[1].getFileName());
  let traps = 0;
  const proxy = new Proxy(sites[0], {
    get() {
      ++traps;
      throw new Error('Proxy CallSite get trap');
    },

    getPrototypeOf() {
      ++traps;
      throw new Error('Proxy CallSite prototype trap');
    }
  });
  const forged = Object.create(Object.getPrototypeOf(sites[0]));
  for (const method of [
    'getFunctionName',
    'getFileName',
    'getLineNumber',
    'getColumnNumber',
    'getScriptNameOrSourceURL',
    'toString',
    'isConstructor',
    'isNative'
  ]) {
    for (const receiver of [undefined, null, {}, forged, proxy]) {
      assert.throws(() => sites[0][method].call(receiver), TypeError);
    }
  }
  assert.equal(traps, 0);
  console.log(
    'PASS captureStackTrace genuine CallSite names locations order and borrowed method receiver branding'
  );

  function ConstructorCapture() {
    Error.captureStackTrace(this);
  }

  const constructed = new ConstructorCapture().stack;
  assert.equal(constructed[0].getFunctionName(), 'ConstructorCapture');
  if (process.versions.mite) {
    assert.throws(
      () => constructed[0].isConstructor(),
      (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
    );
  } else {
    assert.equal(constructed[0].isConstructor(), true);
  }
  const ordinary = {};
  ConstructorCapture.call(ordinary);
  assert.equal(ordinary.stack[0].getFunctionName(), 'ConstructorCapture');
  if (process.versions.mite) {
    assert.throws(
      () => ordinary.stack[0].isConstructor(),
      (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
    );
  } else {
    assert.equal(ordinary.stack[0].isConstructor(), false);
  }
  const weakGet = WeakMap.prototype.get;
  const weakSet = WeakMap.prototype.set;
  try {
    WeakMap.prototype.get = () => {
      throw new Error('Public WeakMap get must not replace private CallSite lookup');
    };

    WeakMap.prototype.set = () => {
      throw new Error('Public WeakMap set must not replace private CallSite admission');
    };

    assert.equal(sites[0].getFunctionName(), 'stackCaptureLeaf');
    assert.equal(stackCaptureTop({}).stack[0].getFunctionName(), 'stackCaptureLeaf');
  } finally {
    WeakMap.prototype.get = weakGet;
    WeakMap.prototype.set = weakSet;
  }
  if (process.versions.mite) {
    for (const method of [
      'getThis',
      'isConstructor',
      'getFunction',
      'getTypeName',
      'getMethodName',
      'getEvalOrigin',
      'isEval',
      'isAsync',
      'isToplevel',
      'isPromiseAll',
      'getPromiseIndex'
    ]) {
      assert.throws(
        () => sites[0][method](),
        (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
      );

      assert.throws(() => sites[0][method].call(proxy), TypeError);
    }
    assert.equal(traps, 0);

    Error.prepareStackTrace = (_target, frames) => frames[0].getThis();

    const unsupportedFormatter = stackCaptureTop({});

    assert.throws(
      () => unsupportedFormatter.stack,
      (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
    );

    Error.prepareStackTrace = (_target, frames) => frames;

    assert.equal(unsupportedFormatter.stack[0].getFunctionName(), 'stackCaptureLeaf');
  }
  console.log(
    'PASS captureStackTrace Node constructor context oracle private intrinsics and explicit Mite metadata limits'
  );
} finally {
  if (original) {
    Object.defineProperty(Error, 'prepareStackTrace', original);
  } else {
    delete Error.prepareStackTrace;
  }
}
