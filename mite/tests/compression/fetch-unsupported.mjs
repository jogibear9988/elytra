/** Do not expose encoded bytes as content when an unavailable algorithm is selected. */
let rejected = false;
try {
  await fetch(`http://127.0.0.1:${process.argv[2]}/unsupported`);
} catch (error) {
  rejected = error instanceof TypeError && error.cause?.code === 'ERR_MITE_UNSUPPORTED_API';
}
if (!rejected) {
  throw new Error('Unavailable encoding did not reject');
}
console.log('PASS unavailable content coding rejects explicitly');
