/** Fatal callback errors prevent the remainder of the batch from running. */
setImmediate(() => {
  throw Error('immediate failure fixture');
});

setImmediate(() => console.log('UNEXPECTED'));
