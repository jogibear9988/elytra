/** Signal deadlines do not keep an otherwise idle runtime alive. */
AbortSignal.timeout(5000).onabort = () => console.log('UNEXPECTED');

console.log('exit');
