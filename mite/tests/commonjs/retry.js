globalThis.attempts = (globalThis.attempts || 0) + 1;
if (globalThis.attempts === 1) {
  throw new Error('retry fixture');
}
module.exports = globalThis.attempts;
