/** Actual named child exits, with no windows, network, or injected crash signal. */
import { app, utilityProcess } from 'firefly';
import path from 'node:path';
const watchdog = setTimeout(() => app.exit(1), 8000);

app
  .whenReady()
  .then(async () => {
    const events = [];

    app.on('child-process-gone', (_event, details) => {
      if (details.type === 'Utility') {
        events.push(details);
      }
    });

    for (const status of [0, 7]) {
      const name = `elytra-owned-service-name-${status}`;
      let pid;
      await new Promise((resolve, reject) => {
        const child = utilityProcess.fork(
          path.join(import.meta.dirname, 'service-name-child.mjs'),
          [String(status)],
          {
            serviceName: name,
            stdio: 'ignore'
          }
        );

        child.once('spawn', () => {
          pid = child.pid;
        });

        child.once('exit', (code) => {
          const gone = events.find((details) => details.name === name);
          if (
            code !== status ||
            !Number.isInteger(pid) ||
            pid < 1 ||
            child.pid !== undefined ||
            (status === 0
              ? gone !== undefined
              : !gone ||
                gone.pid !== pid ||
                gone.exitCode !== status ||
                gone.reason !== 'abnormal-exit')
          ) {
            reject(
              new Error(
                `Named utility exit mismatch: status=${status}, code=${code}, pid=${pid}, gone=${JSON.stringify(gone)}`
              )
            );
            return;
          }
          resolve();
        });
      });
    }
    if (events.length !== 1) {
      throw new Error('Unexpected native child-process-gone events');
    }
    clearTimeout(watchdog);
    console.log('PASS actual utility serviceName normal and abnormal exit attribution');
    app.exit(0);
  })
  .catch((error) => {
    console.error(error);
    app.exit(1);
  });
