/** Fetch has no script destination and must retain the upstream file-read refusal. */
try {
  await fetch(new URL('./not-javascript.txt', import.meta.url));
  document.title = 'FAIL:file fetch became readable';
} catch {
  document.title = 'FETCH_DENIED';
}
