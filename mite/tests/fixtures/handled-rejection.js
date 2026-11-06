const rejected = Promise.reject(new Error('handled'));

Promise.resolve().then(() => rejected.catch((error) => console.log(error.message)));
