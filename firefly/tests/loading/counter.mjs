/** Live bindings and module-map identity are checked by static and dynamic import. */
globalThis.counterEvaluations = (globalThis.counterEvaluations || 0) + 1;
export let count = 0;

export function increment() {
  count++;
}
