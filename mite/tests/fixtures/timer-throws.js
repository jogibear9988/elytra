setTimeout(() => {
  throw new Error('timer fixture');
}, 1);

setTimeout(() => console.log('WRONG timer after exception'), 1);
