const original = require('events');
require.cache.events = { exports: { fake: true } };
console.log(require('events').fake, require('node:events') === original);
delete require.cache.events;
console.log(require.resolve('events'), require.resolve('node:events'), require('events') === original);
try { require('node:unknown'); } catch (error) { console.log(error.code); }
try { require.resolve('node:unknown'); } catch (error) { console.log(error.code); }
