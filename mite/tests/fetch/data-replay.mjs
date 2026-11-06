/** Expose the Node reference's changed-body/old-boundary redirect defect explicitly. */
const form = new FormData();
form.append('field', 'replayed');
form.append('file', new File(['file'], 'x.txt', { type: 'text/plain' }));
const response = await fetch(`http://127.0.0.1:${process.argv[2]}/redirect-form`, {
  method: 'POST',
  body: form
});
const result = await response.json();
if (
  result.validBoundary &&
  (result.parts[0].text !== 'replayed' ||
    result.parts[1].filename !== 'x.txt' ||
    result.parts[1].size !== 4)
) {
  throw new Error('Corrupt multipart replay');
}
console.log('multipart replay boundary', result.validBoundary);
console.log('DONE multipart replay');
