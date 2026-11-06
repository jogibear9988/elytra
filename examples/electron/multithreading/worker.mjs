self.onmessage = (event) => {
  const limit = event.data;
  if (!Number.isInteger(limit) || limit < 0 || limit > 100000) {
    throw Error('Invalid bound');
  }
  let sum = 0;
  for (let i = 1; i <= limit; ++i) {
    sum += i;
  }
  self.postMessage(sum);
};
