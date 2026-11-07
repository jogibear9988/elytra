/** Main-handler acknowledgement, independent of renderer-local title/budget events. */
export function createReceipt() {
  let acknowledge;
  const received = new Promise((resolve) => {
    acknowledge = resolve;
  });
  return {
    acknowledge,

    async wait(diagnostic) {
      let deadline;
      try {
        await Promise.race([
          received,
          new Promise((resolve, reject) => {
            deadline = setTimeout(() => reject(Error(diagnostic())), 5000);
          })
        ]);
      } finally {
        clearTimeout(deadline);
      }
    }
  };
}
