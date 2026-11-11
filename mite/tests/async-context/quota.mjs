/** Mite's explicit bounded context policy; Node has no corresponding 256 limit. */
import assert from 'node:assert/strict';
import { AsyncLocalStorage } from 'node:async_hooks';
const outer = new AsyncLocalStorage();
const others = Array.from({ length: 256 }, () => new AsyncLocalStorage());

outer.run('prior', () => {
  assert.equal(
    outer.run('temporary', () => {
      outer.disable();
      for (let i = 0; i < 255; ++i) {
        others[i].enterWith(i);
      }

      assert.throws(() => others[255].enterWith(255), RangeError);

      assert.equal(others[255].getStore(), undefined);
      return 17;
    }),
    17
  );
  assert.equal(outer.getStore(), 'prior');
  for (let i = 0; i < 255; ++i) {
    assert.equal(others[i].getStore(), i);
  }
});

assert.equal(outer.getStore(), undefined);
console.log('PASS restoration capacity reserved before admission');
