// Native logging uses engine class labels, not user-defined inspection/coercion.
let effects = 0;
const object = Object.create(null);
Object.defineProperty(object, 'toString', {
  get() {
    effects++;
    throw new Error('console invoked a getter');
  }
});

object[Symbol.toPrimitive] = () => {
  effects++;
  throw new Error('console invoked coercion');
};

const proxy = new Proxy(object, {
  get() {
    effects++;
    throw new Error('console invoked a Proxy trap');
  },

  getPrototypeOf() {
    effects++;
    throw new Error('console inspected a Proxy prototype');
  }
});
const revoked = Proxy.revocable({}, {});
revoked.revoke();

console.log(object, [object], function sample() {}, proxy, revoked.proxy);

console.warn(object);
console.error(proxy);
console.error(new TypeError('native error diagnostic'));
const accessorError = new Error('replaced message');
for (const key of ['name', 'message', 'stack']) {
  Object.defineProperty(accessorError, key, {
    get() {
      effects++;
      throw new Error(`console read error ${key}`);
    }
  });
}
console.error(accessorError);
console.log('primitives', 'text', 42, true, null, undefined, 12n);
if (effects !== 0) {
  throw new Error(`Logging produced ${effects} side effects`);
}
console.log('PASS safe native console object logging');
