/** Share private named-result metadata with this realm's promisify implementation. */
export default function namedPromisify(original, names, state) {
  // Match callback result shapes without installing a public custom implementation:
  // repeated promisify(original) must still create distinct wrappers.
  Object.defineProperty(original, state.argumentNames, { value: names });
}
