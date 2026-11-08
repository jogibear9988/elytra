/** The remaining cryptographic algorithms fail explicitly until genuine WebCrypto support lands. */
let failure;
try {
  void crypto.subtle;
} catch (error) {
  failure = error;
}
if (failure?.code !== 'ERR_MITE_UNSUPPORTED_API') {
  throw Error('subtle algorithms falsely advertised');
}
console.log('PASS WebCrypto subtle algorithms explicitly unsupported');
// Mite intentionally uses engine slots even when public view accessors are shadowed.
// Node 24 reads some public properties here; this is not a differential parity claim.
const shadowed = new Uint16Array(16);
for (const name of ['buffer', 'byteLength', 'byteOffset', 'length']) {
  Object.defineProperty(shadowed, name, {
    get() {
      throw Error('Public view property used');
    }
  });
}
if (crypto.getRandomValues(shadowed) !== shadowed) {
  throw Error('Native view identity');
}
console.log('PASS Mite intrinsic view validation ignores shadowed public accessors');
