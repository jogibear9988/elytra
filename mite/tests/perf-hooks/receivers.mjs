/** Match modern Node's receiver rejection and read-only origin behavior. */
import { performance } from 'node:perf_hooks';

function denied(callback, code) {
  try {
    callback();
  } catch (error) {
    if (error instanceof TypeError && error.code === code) {
      return;
    }
    throw error;
  }
  throw new Error('Invalid performance receiver accepted');
}

const now = performance.now;
for (const receiver of [undefined, null, {}, Object.create(performance)]) {
  denied(() => now.call(receiver), 'ERR_INVALID_THIS');
}
let prototype = performance;
while (prototype && !Object.hasOwn(prototype, 'timeOrigin')) {
  prototype = Object.getPrototypeOf(prototype);
}
const getter = Object.getOwnPropertyDescriptor(prototype, 'timeOrigin').get;

denied(() => getter.call({}), 'ERR_INVALID_THIS');

const proxy = new Proxy(performance, {});
if (!(now.call(proxy) >= 0) || getter.call(proxy) !== performance.timeOrigin) {
  throw new Error('Transparent performance proxy rejected');
}
const revoked = Proxy.revocable(performance, {});
revoked.revoke();

denied(() => now.call(revoked.proxy), undefined);

denied(() => {
  performance.timeOrigin = 1;
}, undefined);

console.log('PASS performance method/accessor receiver and read-only origin errors');
