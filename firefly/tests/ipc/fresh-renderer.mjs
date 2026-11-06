/** A newly created document gets its own endpoint and Promise continuations. */
try {
  const first = await desktop.fresh();
  if (first !== true) {
    throw new Error(JSON.stringify(first));
  }
  history.replaceState(null, '', '#same-document');
  const second = await desktop.fresh();
  if (second !== true) {
    throw new Error(JSON.stringify(second));
  }
  document.title = 'FRESH_READY';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
