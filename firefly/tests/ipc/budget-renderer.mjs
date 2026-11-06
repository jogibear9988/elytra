/** The fifth window must reject promptly, then recover after earlier requests finish. */
try {
  if (await desktop.saturate()) {
    document.title = 'BUDGET_ACCEPTED';
  } else {
    document.title = 'BUDGET_REJECTED';
    await new Promise((resolve) => setTimeout(resolve, 200));
    if (!(await desktop.recover())) {
      throw Error('Budget did not recover');
    }
    document.title = 'BUDGET_RECOVERED';
  }
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
