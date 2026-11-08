/** Parent teardown must reap a live utility child without the caller invoking kill(). */
import { app, utilityProcess } from 'firefly';
import fs from 'node:fs';
import path from 'node:path';

let child;
const watchdog = setTimeout(() => {
  child?.kill();
  console.error('FAIL utility parent teardown timed out');
  app.exit(1);
}, 10000);

app
  .whenReady()
  .then(() => {
    child = utilityProcess.fork(path.join(import.meta.dirname, 'child.mjs'), ['kill'], {
      stdio: 'ignore'
    });

    child.once('spawn', () => {
      fs.writeFileSync(process.argv[2], String(child.pid));
      // The runner observes the child while alive, then verifies native teardown reaps it.
      setTimeout(() => {
        clearTimeout(watchdog);
        console.log('PASS utility parent exiting with live child');
        app.exit(0);
      }, 1500);
    });
  })
  .catch((error) => {
    clearTimeout(watchdog);
    child?.kill();
    console.error(error.stack ?? error);
    app.exit(1);
  });
