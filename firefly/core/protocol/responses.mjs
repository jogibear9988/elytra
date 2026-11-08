/** Private modern-handler body controller. No native routing or public protocol support is implied. */
export default function createProtocolResponses(transport, error, unsupported) {
  const { Request, Response, Headers, AbortController, ReadableStream, Buffer } = transport;

  const byteLength = Buffer.byteLength.bind(Buffer);

  const get = (Type, name) => Object.getOwnPropertyDescriptor(Type.prototype, name).get;

  const responseStatus = get(Response, 'status');
  const responseType = get(Response, 'type');
  const responseStatusText = get(Response, 'statusText');
  const responseHeaders = get(Response, 'headers');
  const responseBody = get(Response, 'body');
  const responseUsed = get(Response, 'bodyUsed');
  const headerEntries = Headers.prototype.entries;
  const headerGet = Headers.prototype.get;
  const getReader = ReadableStream.prototype.getReader;
  const cancelStream = ReadableStream.prototype.cancel;
  const abort = AbortController.prototype.abort;
  const probe = getReader.call(
    new ReadableStream({
      start(controller) {
        controller.close();
      }
    })
  );
  const readerPrototype = Object.getPrototypeOf(probe);
  const read = readerPrototype.read;
  const cancelReader = readerPrototype.cancel;
  const releaseReader = readerPrototype.releaseLock;
  releaseReader.call(probe);
  const Bytes = Uint8Array;
  const typedPrototype = Object.getPrototypeOf(Bytes.prototype);
  const typedKind = Object.getOwnPropertyDescriptor(typedPrototype, Symbol.toStringTag).get;
  const typedLength = Object.getOwnPropertyDescriptor(typedPrototype, 'byteLength').get;
  const copyBytes = Bytes.prototype.set;
  const sliceBytes = Bytes.prototype.slice;
  const active = new Map();
  let stopped = false;
  const DATA = 'ERR_FIREFLY_PROTOCOL_DATA';
  const LIMIT = 'ERR_FIREFLY_PROTOCOL_LIMIT';

  function canceled() {
    return error('ERR_FIREFLY_PROTOCOL_CANCELED', 'Protocol response ownership was revoked');
  }

  function current(entry) {
    return !stopped && active.get(entry.id) === entry;
  }

  function quiet(action) {
    try {
      Promise.resolve(action()).catch(() => {});
    } catch {
      /* Cancellation remains best effort. */
    }
  }

  function retire(entry, reason) {
    if (active.get(entry.id) === entry) {
      active.delete(entry.id);
    }
    entry.remainder = null;
    entry.handler = null;
    if (!entry.retired) {
      entry.retired = true;
      entry.reason = reason;

      quiet(() => abort.call(entry.controller, reason));

      if (entry.reader) {
        quiet(() => cancelReader.call(entry.reader, reason));
      } else if (entry.body) {
        quiet(() => cancelStream.call(entry.body, reason));
      }
      entry.rejectCancellation(reason);
    }
  }

  function identity(id, generation) {
    if (
      !Number.isSafeInteger(id) ||
      id <= 0 ||
      !Number.isSafeInteger(generation) ||
      generation <= 0
    ) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid protocol request identity');
    }
  }

  function lookup(id, generation) {
    identity(id, generation);
    const entry = active.get(id);
    if (!entry || entry.generation !== generation || stopped) {
      throw canceled();
    }
    return entry;
  }

  function metadata(entry, response) {
    // Captured accessors enforce the genuine Response brand, even for prototype lookalikes.
    const status = responseStatus.call(response);
    const body = responseBody.call(response);
    entry.body = body;
    if (responseType.call(response) === 'error' || status === 0) {
      throw error('ERR_FIREFLY_PROTOCOL_NETWORK', 'Handler returned a network-error response');
    }
    if (!Number.isInteger(status) || status < 200 || status > 599 || responseUsed.call(response)) {
      throw error(DATA, 'Invalid or consumed handler response');
    }
    const headers = responseHeaders.call(response);
    const fields = Array.from(headerEntries.call(headers));
    if (fields.length > 64) {
      throw error(LIMIT, 'Protocol response headers exceed their budget');
    }
    for (const [name, value] of fields) {
      if (
        [
          'connection',
          'transfer-encoding',
          'trailer',
          'upgrade',
          'keep-alive',
          'proxy-connection',
          'te'
        ].includes(name.toLowerCase()) ||
        /[\x00-\x08\x0a-\x1f\x7f]/.test(value)
      ) {
        throw error(DATA, 'Forbidden protocol response header');
      }
    }
    const length = headerGet.call(headers, 'content-length');
    let contentLength = null;
    if (length !== null) {
      if (
        !/^(?:0|[1-9][0-9]*)$/.test(length) ||
        !Number.isSafeInteger(Number(length)) ||
        Number(length) > 68719476736
      ) {
        throw error(DATA, 'Invalid protocol content length');
      }
      contentLength = Number(length);
    }
    const bodyNull = entry.method === 'HEAD' || body === null || [204, 205, 304].includes(status);
    if (
      (status === 204 && contentLength !== null) ||
      (status === 205 && contentLength !== null && contentLength !== 0) ||
      (bodyNull &&
        entry.method !== 'HEAD' &&
        status !== 304 &&
        contentLength !== null &&
        contentLength !== 0)
    ) {
      throw error(DATA, 'Invalid content length for a null response body');
    }
    const wire = {
      schema: 1,
      kind: 'stream-response',
      request: entry.id,
      status,
      length: contentLength,
      head: entry.method === 'HEAD',
      bodyNull,
      headers: fields
    };
    if (byteLength(JSON.stringify(wire), 'utf8') > 32768) {
      throw error(LIMIT, 'Encoded protocol response head exceeds 32 KiB');
    }
    if (bodyNull) {
      if (body) {
        quiet(() => cancelStream.call(body, canceled()));
      }
    } else {
      entry.reader = getReader.call(body);
    }
    entry.bodyNull = bodyNull;
    entry.contentLength = contentLength;
    return {
      status,
      statusText: responseStatusText.call(response),
      headers: fields,
      contentLength,
      bodyNull
    };
  }

  async function start(id, generation, descriptor, handler) {
    identity(id, generation);
    if (stopped) {
      throw canceled();
    }
    if (active.has(id)) {
      throw error(DATA, 'Protocol request ID is already owned');
    }
    if (active.size >= 64) {
      throw error(LIMIT, 'Protocol response quota exhausted');
    }
    if (typeof handler !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Protocol handler must be a function');
    }
    const controller = new AbortController();
    const method = descriptor.method === undefined ? 'GET' : descriptor.method;
    if (!['GET', 'HEAD'].includes(method)) {
      unsupported('protocol request upload/method ' + method);
    }
    const request = new Request(descriptor.url, {
      method,
      headers: descriptor.headers,
      signal: controller.signal
    });
    if (stopped || active.has(id)) {
      throw canceled();
    }
    // Request/header coercion can call application getters and reenter admission.
    if (active.size >= 64) {
      throw error(LIMIT, 'Protocol response quota exhausted');
    }
    let rejectCancellation;
    const cancellation = new Promise((_resolve, reject) => {
      rejectCancellation = reject;
    });
    // The cancellation race always observes this rejection, including synchronous retirement.
    const entry = {
      id,
      generation,
      method,
      controller,
      rejectCancellation,
      cancellation,
      handler,
      reader: null,
      body: null,
      remainder: null,
      offset: 0,
      pulling: false,
      retired: false,
      bytes: 0
    };
    active.set(id, entry);
    const work = Promise.resolve()
      .then(() => {
        if (!current(entry)) {
          throw entry.reason ?? canceled();
        }
        const invoke = entry.handler;
        return invoke(request);
      })
      .then((response) => {
        if (!current(entry)) {
          // A late async handler may return a live stream after cancellation.
          quiet(() => {
            const body = responseBody.call(response);
            if (body) {
              return cancelStream.call(body, entry.reason);
            }
          });

          throw entry.reason ?? canceled();
        }
        try {
          return metadata(entry, response);
        } catch (failure) {
          const reason = [DATA, LIMIT, 'ERR_FIREFLY_PROTOCOL_NETWORK'].includes(failure?.code)
            ? failure
            : error(DATA, 'Invalid handler Response');
          retire(entry, reason);
          throw reason;
        }
      });
    try {
      return await Promise.race([work, cancellation]);
    } catch (failure) {
      retire(entry, failure);
      throw failure;
    } finally {
      entry.handler = null;
    }
  }

  async function pull(id, generation, credit = 65536) {
    const entry = lookup(id, generation);
    if (!Number.isInteger(credit) || credit < 1 || credit > 65536) {
      throw error('ERR_OUT_OF_RANGE', 'Protocol credit must be 1 to 65536 bytes');
    }
    if (entry.pulling) {
      throw error(DATA, 'Only one protocol body pull may be outstanding');
    }
    if (entry.bodyNull) {
      active.delete(id);
      return { chunk: new Bytes(), done: true };
    }
    if (!entry.reader) {
      throw error(DATA, 'Protocol metadata is not ready');
    }
    entry.pulling = true;
    try {
      let empty = 0;
      while (!entry.remainder?.byteLength) {
        const item = await Promise.race([read.call(entry.reader), entry.cancellation]);
        if (!current(entry)) {
          throw entry.reason ?? canceled();
        }
        if (item.done) {
          if (entry.contentLength !== null && entry.bytes !== entry.contentLength) {
            throw error(DATA, 'Protocol body length does not match metadata');
          }
          active.delete(id);
          releaseReader.call(entry.reader);
          entry.reader = null;
          return { chunk: new Bytes(), done: true };
        }
        if (typedKind.call(item.value) !== 'Uint8Array') {
          throw error(DATA, 'Protocol chunks must be Uint8Array');
        }
        const length = typedLength.call(item.value);
        if (length > 1048576) {
          throw error(LIMIT, 'Protocol source chunk exceeds 1 MiB');
        }
        if (!length && ++empty > 64) {
          throw error(LIMIT, 'Too many empty protocol chunks');
        }
        const owned = new Bytes(length);
        copyBytes.call(owned, item.value);
        entry.remainder = owned;
        entry.offset = 0;
      }
      const length = typedLength.call(entry.remainder);
      const size = Math.min(credit, length - entry.offset);
      const chunk = sliceBytes.call(entry.remainder, entry.offset, entry.offset + size);
      entry.offset += size;
      if (entry.offset === length) {
        entry.remainder = null;
      }
      entry.bytes += size;
      if (entry.bytes > 68719476736) {
        throw error(LIMIT, 'Protocol body exceeds 64 GiB');
      }
      if (entry.contentLength !== null && entry.bytes > entry.contentLength) {
        throw error(DATA, 'Protocol body exceeds content length');
      }
      return { chunk, done: false };
    } catch (failure) {
      retire(entry, failure);
      throw failure;
    } finally {
      entry.pulling = false;
    }
  }

  function cancel(id, generation, reason = canceled()) {
    identity(id, generation);
    const entry = active.get(id);
    if (entry?.generation === generation) {
      retire(entry, reason);
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const entry of [...active.values()]) {
      retire(entry, canceled());
    }
  }

  return { start, pull, cancel, stop };
}
