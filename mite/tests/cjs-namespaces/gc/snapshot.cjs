/** Prior assignments retain namespace names even when later replaced by getters. */
exports.first = undefined;
exports.second = undefined;
Object.defineProperty(exports, 'first', {
  enumerable: true,

  get() {
    ++globalThis.snapshotFirstCalls;
    // No global, cache property or WeakRef retains the returned object. The
    // native snapshot's first RootVector slot must protect it during second().
    return { marker: 'ephemeral', nested: { bytes: new Uint8Array([0, 255, 37]) } };
  }
});
Object.defineProperty(exports, 'second', {
  enumerable: true,

  get() {
    ++globalThis.snapshotSecondCalls;
    collectHostGarbage();
    collectHostGarbage();
    return 'collected';
  }
});
