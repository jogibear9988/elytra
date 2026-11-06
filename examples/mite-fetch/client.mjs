/** Read an HTTP(S) response incrementally; run unchanged with Mite or modern Node. */
const response = await fetch(process.argv[2] ?? 'https://example.com');
console.log('status', response.status);
let bytes = 0;
if (response.body !== null) {
  for await (const chunk of response.body) {
    bytes += chunk.byteLength;
  }
}
console.log('bytes', bytes);
