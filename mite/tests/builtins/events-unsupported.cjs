const E = require('events');
try {
  new E({ captureRejections: true });
} catch (error) {
  console.log(error.code);
}
try {
  E.captureRejections = true;
} catch (error) {
  console.log(error.code);
}
