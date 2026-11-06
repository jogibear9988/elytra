/** HTTP/HTTPS fetch exchanges own upload, response pressure and cancellation together. */
export default function createTransport(host, headers, bodies, objects) {
  const { ReadableStream } = host.webStreams;
  const { Headers } = headers;
  const redirectStatuses = new Set([301, 302, 303, 307, 308]);
  const badPorts = new Set([
    1, 7, 9, 11, 13, 15, 17, 19, 20, 21, 22, 23, 25, 37, 42, 43, 53, 69, 77, 79, 87, 95, 101, 102,
    103, 104, 109, 110, 111, 113, 115, 117, 119, 123, 135, 137, 139, 143, 161, 179, 389, 427, 465,
    512, 513, 514, 515, 526, 530, 531, 532, 540, 548, 554, 556, 563, 587, 601, 636, 989, 990, 993,
    995, 1719, 1720, 1723, 2049, 3659, 4045, 4190, 5060, 5061, 6000, 6566, 6665, 6666, 6667, 6668,
    6669, 6697, 10080
  ]);

  function networkError(cause) {
    return new TypeError('fetch failed', { cause });
  }

  function urlFor(value) {
    const url = new host.URL(value);
    if (!['http:', 'https:'].includes(url.protocol)) {
      throw networkError(host.unsupported('Fetch URL scheme'));
    }
    if (url.username || url.password) {
      throw new TypeError('Fetch URLs cannot contain credentials');
    }
    if (badPorts.has(Number(url.port))) {
      throw networkError(new Error('Blocked Fetch port'));
    }
    url.hash = '';
    return url;
  }

  function exchange(entry, url, redirected) {
    return new Promise((resolve, reject) => {
      const api = host.require(url.protocol === 'https:' ? 'node:https' : 'node:http');
      const stream = host.require('node:stream');
      let request;
      let incoming;
      let upload;
      let controller;
      let rawReader;
      let finished = false;
      let uploadDone = entry.body.stream === null;
      let responseDone = false;
      let aborted = false;

      let unsubscribe = () => {};

      function dispose() {
        unsubscribe();

        unsubscribe = () => {};
      }

      function complete() {
        if (uploadDone && responseDone) {
          finished = true;
          dispose();
        }
      }

      function responseComplete() {
        responseDone = true;
        // A peer can finish its response before a streaming upload finishes
        // (for example 413). Stop the producer and release the one-use transport.
        if (!uploadDone && upload) {
          uploadDone = true;

          upload.cancel().catch(() => {});

          request.destroy();
        }
        complete();
      }

      function fail(error) {
        if (finished) {
          return;
        }
        finished = true;
        const reason = aborted ? host.abort.reason(entry.signal) : networkError(error);
        dispose();
        controller?.error(reason);
        reject(reason);
        request?.destroy(error instanceof Error ? error : undefined);
        upload?.cancel(reason).catch(() => {});
      }

      try {
        const fields = new Headers(entry.headers);
        // Fetch derives authority/SNI from its URL. Like the Node reference,
        // ignore a caller's Host header before entering the HTTPS adapter.
        fields.delete('host');
        if (!fields.has('accept')) {
          fields.set('accept', '*/*');
        }
        if (!fields.has('accept-language')) {
          fields.set('accept-language', '*');
        }
        if (!fields.has('user-agent')) {
          fields.set('user-agent', 'mite');
        }
        if (!fields.has('accept-encoding')) {
          fields.set('accept-encoding', 'gzip, deflate');
        }
        if (!fields.has('sec-fetch-mode')) {
          fields.set('sec-fetch-mode', entry.mode);
        }
        if (fields.has('expect') || fields.has('upgrade')) {
          throw host.unsupported('Fetch Expect/Upgrade');
        }
        if (
          entry.body.length !== null &&
          fields.has('content-length') &&
          fields.get('content-length') !== String(entry.body.length)
        ) {
          throw new TypeError('Body length does not match Content-Length');
        }
        if (
          entry.body.stream !== null &&
          entry.body.length !== null &&
          !fields.has('content-length')
        ) {
          fields.set('content-length', String(entry.body.length));
        }
        if (
          entry.body.stream === null &&
          ['POST', 'PUT'].includes(entry.method) &&
          !fields.has('content-length')
        ) {
          fields.set('content-length', '0');
        }
        request = api.request(url, { method: entry.method, headers: [...fields].flat() });
        request.on('error', fail);

        request.on('response', (reply) => {
          incoming = reply;
          // Redirect/null-body branches also own errors while disposing a peer.
          reply.on('error', () => {});

          if (finished) {
            reply.destroy();
            return;
          }
          try {
            const fields = new Headers();
            for (let index = 0; index < reply.rawHeaders.length; index += 2) {
              fields.append(reply.rawHeaders[index], reply.rawHeaders[index + 1]);
            }
            const status = reply.statusCode;
            const location = fields.get('location');
            if (redirectStatuses.has(status) && location !== null && entry.redirect !== 'manual') {
              finished = true;
              dispose();
              request.destroy();
              reply.destroy();
              upload?.cancel().catch(() => {});
              if (entry.redirect === 'error') {
                reject(networkError(new Error('Redirect mode is error')));
              } else {
                resolve({ location, status });
              }
              return;
            }
            let body = null;
            if (entry.method === 'HEAD' || [204, 205, 304].includes(status)) {
              reply.once('end', () => {
                responseComplete();
              });

              reply.resume();
            } else {
              let source = stream.Readable.toWeb(reply, {
                strategy: { highWaterMark: 16384, size: (chunk) => chunk.byteLength }
              });
              const encodings = (fields.get('content-encoding') ?? '')
                .split(',')
                .map((value) => value.trim().toLowerCase())
                .filter((value) => value && value !== 'identity');
              if (
                encodings.some(
                  (value) => !['gzip', 'x-gzip', 'deflate', 'x-deflate'].includes(value)
                )
              ) {
                throw host.unsupported('Fetch content coding');
              }
              if (fields.get('content-length') !== '0') {
                for (const encoding of encodings.reverse()) {
                  source = source.pipeThrough(
                    host.decompression.create(
                      encoding.endsWith('gzip') ? 'gzip-http' : 'auto-deflate'
                    )
                  );
                }
              }
              rawReader = source.getReader();
              body = new ReadableStream({
                type: 'bytes',

                start(value) {
                  controller = value;
                },

                async pull(value) {
                  try {
                    const { value: chunk, done } = await rawReader.read();
                    if (finished) {
                      return;
                    }
                    if (done) {
                      value.close();
                      // An outstanding BYOB read needs its zero-byte EOF response.
                      value.byobRequest?.respond(0);
                      responseComplete();
                    } else {
                      value.enqueue(chunk);
                    }
                  } catch (error) {
                    fail(error);
                  }
                },

                cancel(reason) {
                  finished = true;
                  dispose();
                  upload?.cancel(reason).catch(() => {});
                  return rawReader.cancel(reason);
                }
              });
            }
            resolve({
              response: objects.networkResponse(body, {
                status,
                statusText: reply.statusMessage ?? '',
                headers: fields,
                type: 'basic',
                url: url.href,
                redirected
              })
            });
          } catch (error) {
            fail(error);
          }
        });

        unsubscribe = host.abort.subscribe(entry.signal, () => {
          aborted = true;
          fail(host.abort.reason(entry.signal));
        });
        if (finished) {
          return;
        }
        if (entry.body.stream === null) {
          request.end();
        } else {
          if (bodies.unusable(entry.body)) {
            throw new TypeError('Request body is locked or already used');
          }
          upload = entry.body.stream.getReader();
          entry.body.used = true;
          const writer = stream.Writable.toWeb(request).getWriter();
          (async () => {
            try {
              while (!finished) {
                const { value, done } = await upload.read();
                if (done || finished) {
                  break;
                }
                if (!(value instanceof Uint8Array)) {
                  throw new TypeError('Fetch upload chunks must be Uint8Array');
                }
                await writer.write(value);
              }
              if (!finished) {
                await writer.close();
                uploadDone = true;
                complete();
              }
            } catch (error) {
              fail(error);
            } finally {
              upload.releaseLock();
              writer.releaseLock();
            }
          })().catch(fail);
        }
      } catch (error) {
        fail(error);
      }
    });
  }

  return async function fetch(input, init = undefined) {
    if (arguments.length === 0) {
      throw new TypeError('fetch requires input');
    }
    if (init?.dispatcher !== undefined || init?.agent !== undefined) {
      throw host.unsupported('Fetch custom dispatcher/agent');
    }
    let entry = objects.requestState(new objects.Request(input, init));
    for (let redirects = 0; ; redirects++) {
      if (entry.signal.aborted) {
        const reason = host.abort.reason(entry.signal);
        entry.body.stream?.cancel(reason).catch(() => {});
        throw reason;
      }
      const url = urlFor(entry.url);
      const result = await exchange(entry, url, redirects > 0);
      if (result.response) {
        return result.response;
      }
      if (redirects >= 20) {
        throw networkError(new Error('Too many redirects'));
      }
      const next = urlFor(new host.URL(result.location, url).href);
      if (!new host.URL(result.location, url).hash) {
        next.hash = new host.URL(entry.url).hash;
      }
      const fields = new Headers(entry.headers);
      if (next.origin !== url.origin) {
        for (const name of ['authorization', 'proxy-authorization', 'cookie']) {
          fields.delete(name);
        }
      }
      let method = entry.method;
      let body;
      if (
        (result.status === 303 && method !== 'GET' && method !== 'HEAD') ||
        ([301, 302].includes(result.status) && method === 'POST')
      ) {
        method = 'GET';
        body = bodies.extract(null);
        for (const name of [
          'content-length',
          'content-type',
          'content-encoding',
          'content-language',
          'content-location'
        ]) {
          fields.delete(name);
        }
      } else if (entry.body.stream !== null) {
        if (entry.body.source === null) {
          throw networkError(new Error('Cannot replay streamed request body'));
        }
        body = bodies.replay(entry.body);
      } else {
        body = bodies.extract(null);
      }
      entry = { ...entry, url: next.href, method, body, headers: fields };
    }
  };
}
