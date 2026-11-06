for (const [specifier, options] of [
  ['./data.json', undefined],
  ['./data.json', { with: { type: 'wrong' } }],
  ['./dep.mjs', { with: { type: 'json' } }],
  ['./dep.mjs', { with: { type: 'wrong' } }]
]) {
  try {
    await import(specifier, options);
    console.log('WRONG accepted');
  } catch (error) {
    console.log(error.name, error.code);
  }
}
try {
  await import('./broken.json', { with: { type: 'json' } });
} catch (error) {
  console.log(error.name);
}
console.log('handled');
