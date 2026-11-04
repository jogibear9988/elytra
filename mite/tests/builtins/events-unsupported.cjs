const E = require('events');
try { new E({ captureRejections: true }); }
catch (error) { console.log(error.code); }
try { E.captureRejections = true; }
catch (error) { console.log(error.code); }
E.once(new E(), 'work', { signal: {} }).catch(error => console.log(error.code));
