(async () => {
  try {
    await Promise.reject(new Error('awaited rejection handled'));
  } catch (error) {
    console.log(error.message);
  }
})();
