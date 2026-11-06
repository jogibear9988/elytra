setTimeout(() => Promise.reject(new Error('timer rejection fixture')), 1);

setTimeout(() => console.log('WRONG timer after rejection'), 1);
