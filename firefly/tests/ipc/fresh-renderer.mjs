/** A newly created document gets its own endpoint and Promise continuations. */
try {
  await desktop.fresh();
  history.replaceState(null, '', '#same-document');
  await desktop.fresh();
  document.title = 'FRESH_READY';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
