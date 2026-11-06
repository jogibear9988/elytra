for (const schedule of [queueMicrotask, setTimeout, setInterval]) {
  for (const callback of [undefined, null, 12, 'source', {}]) {
    try {
      schedule(callback);
      console.log('WRONG accepted');
    } catch (error) {
      console.log(error.name, error.code);
    }
  }
}
for (const delay of [Symbol('delay'), 1n]) {
  try {
    setTimeout(() => {}, delay);
  } catch (error) {
    console.log(error.name);
  }
}
