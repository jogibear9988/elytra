/** Actual loopback utility requests and main session cancellation, without windows. */
import { app, utilityProcess, session, net } from 'firefly';
import http from 'node:http';
import path from 'node:path';
const deadline = setTimeout(() => app.exit(1), 10000);
let child;
let server;

app
  .whenReady()
  .then(async () => {
    let hits = 0;
    server = http.createServer((_request, response) => {
      hits++;
      response.end('owned-utility');
    });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    const url = `http://127.0.0.1:${server.address().port}/owned`;
    child = utilityProcess.fork(path.join(import.meta.dirname, 'independent-net-child.mjs'), [], {
      stdio: 'ignore'
    });
    await new Promise((resolve) =>
      child.once('message', (data) => {
        if (!data.ready) {
          throw new Error('Missing genuine child readiness');
        }
        resolve();
      })
    );
    // Registration with a live no-session child must not affect its system context.
    let checked = 0;
    const filter = { urls: [url] };

    session.defaultSession.webRequest.onBeforeRequest(filter, (_details, done) => {
      checked++;
      done({ cancel: true });
    });

    await net.fetch(url).then(
      () => {
        throw new Error('Main request bypassed its session filter');
      },
      () => {}
    );
    if (checked !== 1 || hits !== 0) {
      throw new Error('Main cancellation did not prevent actual endpoint access');
    }
    const reply = new Promise((resolve) => child.once('message', resolve));
    child.postMessage(url);
    const result = await reply;
    if (result.status !== 200 || result.body !== 'owned-utility' || hits !== 1 || checked !== 1) {
      throw new Error(
        `Utility system context mismatch: ${JSON.stringify(result)} hits=${hits} checked=${checked}`
      );
    }
    // A new no-session child can also start while the main filter is installed.
    const second = utilityProcess.fork(
      path.join(import.meta.dirname, 'service-name-child.mjs'),
      ['0'],
      { stdio: 'ignore' }
    );
    await new Promise((resolve) => second.once('exit', resolve));
    await net.fetch(url).then(
      () => {
        throw new Error('Main filter disappeared after utility request');
      },
      () => {}
    );
    if (hits !== 1 || checked !== 2) {
      throw new Error('Utility request changed main filtering');
    }
    session.defaultSession.webRequest.onBeforeRequest(null);
    child.kill();
    server.close();
    clearTimeout(deadline);
    console.log('PASS actual no-session utility net independence and retained main filters');
    app.exit(0);
  })
  .catch((error) => {
    console.error(error);
    child?.kill();
    server?.close();
    app.exit(1);
  });
