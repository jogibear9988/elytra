/** Native deadlines retain listener-only signals until they actually fire. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
let callbacks = 0;

AbortSignal.timeout(100).onabort = () => callbacks++;

AbortSignal.timeout(100).addEventListener('abort', () => callbacks++, { once: true });

AbortSignal.any([AbortSignal.timeout(100)]).onabort = () => callbacks++;

setImmediate(() => collect());

setTimeout(() => console.log('deadlines', callbacks === 3), 180);
