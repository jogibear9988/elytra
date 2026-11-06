/** Publish only the final timeout observation to the native test harness. */
try {
  await desktop.deadline();
  document.title = 'TIMEOUT_OK';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
