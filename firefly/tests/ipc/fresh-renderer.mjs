/** A newly created document gets its own endpoint and Promise continuations. */
try {
  for (let index = 0; index < 8; index++) {
    if (index) {
      history.replaceState(null, '', index % 2 ? '#same-document' : location.pathname);
    }
    // No timer/poll/retry: the main-process URL must be correct for this invoke.
    const result = await desktop.fresh();
    if (result !== true) {
      throw new Error(JSON.stringify(result));
    }
  }
  document.title = 'FRESH_READY';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
