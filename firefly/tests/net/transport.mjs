/** Actual owned HTTP(S) peers exercise the public net transport, never browser windows. */
import http from 'node:http';
import https from 'node:https';

export default async function exerciseNet(net, config) {
  const original = { fetch: globalThis.fetch, http: http.request, https: https.request };

  const replaced = () => {
    throw Error('Public networking replacement intercepted private net transport');
  };

  globalThis.fetch = replaced;
  http.request = replaced;
  https.request = replaced;
  try {
    const { origin, other, secure } = config;

    const check = (value, message) => {
      if (!value) {
        throw Error(message);
      }
    };

    const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

    const waitSeen = async (key) => {
      for (let attempt = 0; attempt < 100; attempt++) {
        if ((await (await net.fetch(origin + '/seen/' + key)).json()).seen) {
          return;
        }
        await sleep(5);
      }
      throw Error('Independent server never observed request: ' + key);
    };

    let response = await net.fetch(origin + '/echo');
    check(
      response.ok && response.status === 200 && response.headers.get('x-peer') === 'owned-loopback',
      'Real fetch response metadata'
    );
    check((await response.json()).method === 'GET', 'Actual fetch GET');
    const bytes = new Uint8Array([0, 255, 128, 1, 13, 10]);
    response = await net.fetch(origin + '/echo', {
      method: 'POST',
      body: bytes,
      headers: { 'Content-Type': 'application/octet-stream', 'X-Probe': 'snapshot' }
    });
    let echo = await response.json();
    check(
      echo.bodyHex === '00ff80010d0a' && echo.method === 'POST' && echo.probe === 'snapshot',
      'Fetch binary upload reaches independent peer'
    );
    response = await net.fetch(origin + '/redirect?to=' + encodeURIComponent(other + '/echo'), {
      headers: { Authorization: 'Bearer private-origin', 'X-Probe': 'cross-origin' }
    });
    echo = await response.json();
    check(echo.probe === 'cross-origin', 'Real cross-origin redirect reaches owned peer');
    const credentialForwarded = echo.authorization !== null;
    if (config.strictOrigin) {
      check(!credentialForwarded, 'Firefly strips cross-origin credentials');
    }
    if (config.strictOrigin) {
      const portRedirect = await net.fetch(
        origin + '/redirect?to=' + encodeURIComponent(config.crossPort + '/echo'),
        { headers: { Authorization: 'Bearer port-private' } }
      );
      check(
        (await portRedirect.json()).authorization === null,
        'Firefly strips origin credentials across port boundaries'
      );
    }
    response = await net.fetch(origin + '/redirect?to=' + encodeURIComponent(origin + '/echo'), {
      headers: { Authorization: 'Bearer same-origin' }
    });
    check(
      (await response.json()).authorization === 'Bearer same-origin',
      'Same-origin redirect retains credentials'
    );
    response = await net.fetch(origin + '/gzip');
    check((await response.text()) === 'decoded Grüße 日本語', 'Real gzip response decoded');
    const abort = new AbortController();
    const key = (config.runId || 'run') + '-' + config.label + '-fetch-abort';
    const held = net.fetch(origin + '/hold/' + key, { signal: abort.signal });
    const failure = held.then(
      () => {
        throw Error('Canceled fetch succeeded');
      },
      (error) => error
    );
    await waitSeen(key);
    abort.abort();
    check((await failure).name === 'AbortError', 'Fetch cancellation rejects before headers');
    await net.fetch(origin + '/release/' + key);
    const bodyKey = (config.runId || 'run') + '-' + config.label + '-body-abort';
    const bodyAbort = new AbortController();
    const streamed = await net.fetch(origin + '/stream/' + bodyKey, { signal: bodyAbort.signal });
    const reader = streamed.body.getReader();
    check(
      (await reader.read()).value.byteLength > 0,
      'Native fetch streams before response completion'
    );
    bodyAbort.abort();
    let bodyFailure;
    try {
      await reader.read();
    } catch (error) {
      bodyFailure = error.name;
    }
    check(bodyFailure === 'AbortError', 'Fetch cancellation reaches response-body reader');
    await net.fetch(origin + '/release/' + bodyKey);

    let tlsFailed = false;
    try {
      await net.fetch(secure + '/untrusted');
    } catch {
      tlsFailed = true;
    }
    check(tlsFailed, 'Untrusted HTTPS certificate is rejected');

    async function request(options, body, inspect) {
      return new Promise((resolve, reject) => {
        const chunks = [];
        const events = [];
        const req = net.request(options, (res) => {
          events.push('response');
          check(
            Number.isInteger(res.statusCode) && typeof res.headers === 'object',
            'Real request response facade'
          );
          res.on('error', reject);

          res.on('data', (chunk) => {
            check(Buffer.isBuffer(chunk), 'Real response chunks are buffers');
            chunks.push(chunk);
          });

          res.on('end', () => resolve({ res, bytes: Buffer.concat(chunks), events }));

          if (inspect) {
            inspect(res);
          }
        });
        req.on('error', reject);

        req.on('finish', () => events.push('finish'));

        req.on('close', () => events.push('close'));

        req.setHeader('X-Probe', 'request-header');
        check(req.getHeader('X-Probe') === 'request-header', 'Request header snapshot');
        req.setHeader('X-Remove', 'removed');
        req.removeHeader('X-Remove');
        check(req.getHeader('X-Remove') === undefined, 'Request header removal');
        if (body) {
          let inline = true;

          req.write(body, (error) => {
            check(!inline && !error, 'Upload write callback asynchronous');
            body.fill(9);
            req.end();
          });

          inline = false;
        } else {
          req.end();
        }
      });
    }

    let incoming = await request(
      { url: origin + '/echo', method: 'POST' },
      Buffer.from([0, 255, 128, 7])
    );
    echo = JSON.parse(incoming.bytes.toString());
    check(
      echo.bodyHex === '00ff8007' && echo.probe === 'request-header',
      'Actual ClientRequest byte upload'
    );
    check(
      incoming.events.includes('finish') && incoming.events.includes('response'),
      'Request upload/response lifecycle'
    );
    incoming = await request(origin + '/gzip');
    check(
      incoming.bytes.toString() === 'decoded Grüße 日本語',
      'ClientRequest decompression matches Electron'
    );
    let pausedBytes = 0;
    incoming = await request(origin + '/large', undefined, (res) => {
      res.pause();

      res.on('data', (chunk) => (pausedBytes += chunk.length));

      setTimeout(() => {
        check(pausedBytes === 0, 'Paused response does not deliver application data');
        res.resume();
      }, 30);
    });
    check(
      incoming.bytes.length === 2 * 1024 * 1024 &&
        incoming.bytes[0] === 0 &&
        incoming.bytes.at(-1) === 255,
      'Paused/resumed native response preserves body bytes'
    );

    let redirectEvents = 0;
    const manuallyFollowed = await new Promise((resolve, reject) => {
      const req = net.request(
        { url: origin + '/redirect?to=' + encodeURIComponent(other + '/echo'), redirect: 'manual' },
        (res) => {
          const chunks = [];
          res.on('error', reject);

          res.on('data', (value) => chunks.push(value));

          res.on('end', () => resolve(JSON.parse(Buffer.concat(chunks).toString())));
        }
      );
      req.on('error', reject);

      req.on('redirect', (status, method, target, headers) => {
        check(
          status === 302 &&
            method === 'GET' &&
            target === other + '/echo' &&
            typeof headers === 'object',
          'Actual manual redirect event metadata'
        );
        redirectEvents++;
        req.followRedirect();
      });

      req.end();
    });
    check(
      redirectEvents === 1 && manuallyFollowed.method === 'GET',
      'Synchronous manual redirect continuation'
    );
    const reqKey = (config.runId || 'run') + '-' + config.label + '-request-abort';
    let reqAborted = 0;
    let reqResponded = false;
    const req = net.request(origin + '/hold/' + reqKey);

    req.on('response', (res) => {
      reqResponded = true;
      res.resume();
    });

    req.on('error', () => {});

    req.on('abort', () => reqAborted++);

    req.end();
    await waitSeen(reqKey);
    req.abort();
    await net.fetch(origin + '/release/' + reqKey);
    await sleep(20);
    check(reqAborted === 1 && !reqResponded, 'Request abort retires pending headers');
    return {
      fetchBytes: true,
      redirects: true,
      credentialForwarded,
      decoded: true,
      fetchAbort: true,
      fetchBodyAbort: true,
      capturedTransport: true,
      tlsRejected: true,
      requestBytes: true,
      responsePause: true,
      requestAbort: true,
      manualRedirect: true
    };
  } finally {
    globalThis.fetch = original.fetch;
    http.request = original.http;
    https.request = original.https;
  }
}
