/** Upload an in-memory File using the standard Fetch API under Mite or Node. */
if (!process.argv[2]) {
  throw new Error('Usage: mite multipart.mjs <HTTP(S) upload endpoint>');
}
const form = new FormData();
form.append('project', 'Mite + Firefly');
form.append(
  'file',
  new File([JSON.stringify({ runtime: 'LibJS' })], 'project.json', {
    type: 'application/json'
  })
);
const response = await fetch(process.argv[2], { method: 'POST', body: form });
console.log('status', response.status);
console.log(await response.text());
