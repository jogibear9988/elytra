/* Generated from mite/runtime/bootstrap/fetch/index.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/runtime/bootstrap/fetch/headers.mjs
function createHeaders() {
  const states = /* @__PURE__ */ new WeakMap();
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;
  function bytes(value2) {
    if (typeof value2 === "symbol") {
      throw new TypeError("Cannot convert Symbol to a header string");
    }
    const text = String(value2);
    for (let index = 0; index < text.length; index++) {
      if (text.charCodeAt(index) > 255) {
        throw new TypeError("Header strings must contain bytes");
      }
    }
    return text;
  }
  function name(value2) {
    const text = bytes(value2);
    if (!token.test(text)) {
      throw new TypeError("Invalid header name");
    }
    return text.toLowerCase();
  }
  function value(input) {
    const text = bytes(input).replace(/^[\t\n\r ]+|[\t\n\r ]+$/g, "");
    if (/[\0\r\n]/.test(text)) {
      throw new TypeError("Invalid header value");
    }
    return text;
  }
  function state(headers, mutate = false) {
    const entry = states.get(headers);
    if (!entry) {
      throw new TypeError("Expected Headers receiver");
    }
    if (mutate && entry.immutable) {
      throw new TypeError("Headers are immutable");
    }
    return entry;
  }
  function pairs(headers) {
    const grouped = /* @__PURE__ */ new Map();
    for (const [key, value2] of state(headers).list) {
      const values = grouped.get(key) ?? [];
      values.push(value2);
      grouped.set(key, values);
    }
    return [...grouped.keys()].sort().flatMap(
      (key) => key === "set-cookie" ? grouped.get(key).map((value2) => [key, value2]) : [[key, grouped.get(key).join(key === "cookie" ? "; " : ", ")]]
    );
  }
  class Headers {
    constructor(init = void 0) {
      states.set(this, { list: [], immutable: false });
      if (init === void 0) {
        return;
      }
      if (init === null || typeof init !== "object" && typeof init !== "function") {
        throw new TypeError("Header initializer must be a record or sequence");
      }
      if (init[Symbol.iterator] !== void 0) {
        for (const pair of init) {
          if (!pair || typeof pair !== "object" && typeof pair !== "function" || typeof pair[Symbol.iterator] !== "function") {
            throw new TypeError("Invalid header pair");
          }
          const values = [...pair];
          if (values.length !== 2) {
            throw new TypeError("Header pair must have two entries");
          }
          this.append(values[0], values[1]);
        }
      } else {
        for (const key of Object.keys(init)) {
          this.append(key, init[key]);
        }
      }
    }
    append(key, input) {
      if (arguments.length < 2) {
        throw new TypeError("append requires name and value");
      }
      const field = name(key);
      const text = value(input);
      state(this, true).list.push([field, text]);
    }
    set(key, input) {
      if (arguments.length < 2) {
        throw new TypeError("set requires name and value");
      }
      const field = name(key);
      const text = value(input);
      const entry = state(this, true);
      const index = entry.list.findIndex(([key2]) => key2 === field);
      entry.list = entry.list.filter(([key2]) => key2 !== field);
      entry.list.splice(index < 0 ? entry.list.length : index, 0, [field, text]);
    }
    delete(key) {
      if (!arguments.length) {
        throw new TypeError("delete requires a name");
      }
      const field = name(key);
      const entry = state(this, true);
      entry.list = entry.list.filter(([key2]) => key2 !== field);
    }
    get(key) {
      if (!arguments.length) {
        throw new TypeError("get requires a name");
      }
      const field = name(key);
      const values = state(this).list.filter(([key2]) => key2 === field).map(([, value2]) => value2);
      return values.length ? values.join(field === "cookie" ? "; " : ", ") : null;
    }
    has(key) {
      if (!arguments.length) {
        throw new TypeError("has requires a name");
      }
      const field = name(key);
      return state(this).list.some(([key2]) => key2 === field);
    }
    getSetCookie() {
      return state(this).list.filter(([key]) => key === "set-cookie").map(([, value2]) => value2);
    }
    *entries() {
      state(this);
      for (let index = 0; ; index++) {
        const list = pairs(this);
        if (index >= list.length) {
          return;
        }
        yield list[index];
      }
    }
    *keys() {
      for (const [key] of this.entries()) {
        yield key;
      }
    }
    *values() {
      for (const [, value2] of this.entries()) {
        yield value2;
      }
    }
    forEach(callback, thisArg = void 0) {
      state(this);
      if (typeof callback !== "function") {
        throw new TypeError("callback must be a function");
      }
      for (const [key, value2] of this.entries()) {
        callback.call(thisArg, value2, key, this);
      }
    }
  }
  Headers.prototype[Symbol.iterator] = Headers.prototype.entries;
  Object.defineProperty(Headers.prototype, Symbol.toStringTag, {
    value: "Headers",
    configurable: true
  });
  return {
    Headers,
    isImmutable: (headers) => state(headers).immutable,
    immutable(headers) {
      state(headers).immutable = true;
      return headers;
    }
  };
}

// mite/runtime/bootstrap/fetch/body.mjs
function createBodies(host) {
  const { ReadableStream } = host.webStreams;
  const Buffer = host.Buffer;
  const encoder = new host.TextEncoder();
  const locked = Object.getOwnPropertyDescriptor(ReadableStream.prototype, "locked").get;
  function isStream(value) {
    try {
      locked.call(value);
      return true;
    } catch {
      return false;
    }
  }
  function used(body) {
    return body.used || body.stream !== null && host.streamState.isDisturbed(body.stream);
  }
  function unusable(body) {
    return used(body) || body.stream !== null && locked.call(body.stream);
  }
  function fromBytes(bytes2, type = null) {
    return {
      stream: new ReadableStream({
        type: "bytes",
        start(controller) {
          if (bytes2.byteLength) {
            controller.enqueue(bytes2.slice());
          }
          controller.close();
        }
      }),
      source: bytes2,
      length: bytes2.byteLength,
      type,
      used: false
    };
  }
  function extract(input) {
    if (typeof input === "symbol") {
      throw new TypeError("Cannot convert Symbol to a body string");
    }
    if (input === null || input === void 0) {
      return { stream: null, source: null, length: 0, type: null, used: false };
    }
    if (host.data.isBlob(input)) {
      return fromBlob(input);
    }
    if (host.data.isFormData(input)) {
      return fromBlob(host.data.formDataToBlob(input));
    }
    if (isStream(input)) {
      const body = { stream: input, source: null, length: null, type: null, used: false };
      if (unusable(body)) {
        throw new TypeError("Body stream is locked or already used");
      }
      return body;
    }
    if (input instanceof ArrayBuffer) {
      return fromBytes(new Uint8Array(input).slice());
    }
    if (ArrayBuffer.isView(input)) {
      return fromBytes(new Uint8Array(input.buffer, input.byteOffset, input.byteLength).slice());
    }
    if (input instanceof host.URLSearchParams) {
      return fromBytes(
        encoder.encode(input.toString()),
        "application/x-www-form-urlencoded;charset=UTF-8"
      );
    }
    if (input && typeof input === "object" && (typeof input.stream === "function" || Object.prototype.toString.call(input) === "[object FormData]")) {
      throw host.unsupported("Foreign Blob/File/FormData bodies");
    }
    return fromBytes(encoder.encode(String(input)), "text/plain;charset=UTF-8");
  }
  function fromBlob(blob) {
    return {
      stream: host.data.stream(blob),
      source: blob,
      length: host.data.size(blob),
      type: host.data.type(blob) || null,
      used: false
    };
  }
  function replay(body) {
    return host.data.isBlob(body.source) ? fromBlob(body.source) : fromBytes(body.source.slice(), body.type);
  }
  function clone(body) {
    if (unusable(body)) {
      throw new TypeError("Body is locked or already used");
    }
    if (body.stream === null) {
      return { ...body };
    }
    const [left, right] = body.stream.tee();
    body.stream = left;
    return { ...body, stream: right };
  }
  function transfer(body) {
    if (unusable(body)) {
      throw new TypeError("Body is locked or already used");
    }
    if (body.stream === null) {
      return { ...body };
    }
    const reader = body.stream.getReader();
    body.used = true;
    return {
      ...body,
      used: false,
      stream: new ReadableStream({
        async pull(controller) {
          try {
            const { value, done } = await reader.read();
            if (done) {
              controller.close();
            } else {
              controller.enqueue(value);
            }
          } catch (error) {
            controller.error(error);
          }
        },
        cancel(reason) {
          return reader.cancel(reason);
        }
      })
    };
  }
  async function bytes(body) {
    if (unusable(body)) {
      throw new TypeError("Body is locked or already used");
    }
    if (body.stream === null) {
      return new Uint8Array();
    }
    body.used = true;
    const reader = body.stream.getReader();
    const chunks = [];
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          break;
        }
        if (!(value instanceof Uint8Array)) {
          throw new TypeError("Body stream chunks must be Uint8Array");
        }
        chunks.push(Buffer.from(value));
      }
      return new Uint8Array(Buffer.concat(chunks));
    } catch (error) {
      reader.cancel(error).catch(() => {
      });
      throw error;
    } finally {
      reader.releaseLock();
    }
  }
  function install(Type, state) {
    Object.defineProperties(Type.prototype, {
      body: {
        configurable: true,
        enumerable: true,
        get() {
          return state(this).body.stream;
        }
      },
      bodyUsed: {
        configurable: true,
        enumerable: true,
        get() {
          return used(state(this).body);
        }
      }
    });
    for (const [name, convert] of Object.entries({
      bytes: (value) => value,
      arrayBuffer: (value) => value.buffer,
      text: (value) => new host.TextDecoder().decode(value),
      json: (value) => JSON.parse(new host.TextDecoder().decode(value))
    })) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        writable: true,
        value: async function() {
          return convert(await bytes(state(this).body));
        }
      });
    }
    for (const name of ["blob", "formData"]) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        writable: true,
        value: async function() {
          const entry = state(this);
          const type = entry.headers.get("content-type");
          const value = await bytes(entry.body);
          return name === "blob" ? new host.data.Blob([value], { type: host.data.mimeType(type) }) : host.data.parseFormData(type, value);
        }
      });
    }
  }
  return { extract, clone, transfer, used, unusable, fromBytes, isStream, install, replay };
}

// mite/runtime/bootstrap/fetch/objects.mjs
function createObjects(host, headers, bodies) {
  const { Headers } = headers;
  const requests = /* @__PURE__ */ new WeakMap();
  const responses = /* @__PURE__ */ new WeakMap();
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;
  const emptyStatuses = /* @__PURE__ */ new Set([101, 103, 204, 205, 304]);
  const redirects = /* @__PURE__ */ new Set([301, 302, 303, 307, 308]);
  function dictionary(value) {
    if (value === null || value === void 0) {
      return {};
    }
    if (typeof value !== "object" && typeof value !== "function") {
      throw new TypeError("Expected an options dictionary");
    }
    return value;
  }
  function state(map, value) {
    const result = map.get(value);
    if (!result) {
      throw new TypeError("Invalid Fetch object receiver");
    }
    return result;
  }
  function choice(value, fallback, values, name) {
    const text = value === void 0 ? fallback : String(value);
    if (!values.includes(text)) {
      throw new TypeError(`Invalid ${name}`);
    }
    return text;
  }
  function statusText(value) {
    const text = value === void 0 ? "" : String(value);
    if (/[^\t\x20-\x7E\x80-\xFF]/.test(text)) {
      throw new TypeError("Invalid statusText");
    }
    return text;
  }
  function initializeBody(body, target) {
    if (body.type !== null && !target.has("content-type")) {
      target.set("content-type", body.type);
    }
    return body;
  }
  function properties(Type, map, names) {
    for (const name of names) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        enumerable: true,
        get() {
          return state(map, this)[name];
        }
      });
    }
    Object.defineProperty(Type.prototype, Symbol.toStringTag, {
      value: Type.name,
      configurable: true
    });
    bodies.install(Type, (value) => state(map, value));
  }
  class Request {
    constructor(input, init = void 0) {
      if (!arguments.length) {
        throw new TypeError("Request requires input");
      }
      const opts = dictionary(init);
      const parent = requests.get(input);
      const url = new host.URL(parent?.url ?? String(input));
      if (url.username || url.password) {
        throw new TypeError("Request URLs cannot contain credentials");
      }
      let method = String(opts.method === void 0 ? parent?.method ?? "GET" : opts.method);
      if (!token.test(method) || ["CONNECT", "TRACE", "TRACK"].includes(method.toUpperCase())) {
        throw new TypeError("Invalid or forbidden method");
      }
      if (["DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT"].includes(method.toUpperCase())) {
        method = method.toUpperCase();
      }
      const entry = {
        url: url.href,
        method,
        headers: new Headers(opts.headers === void 0 ? parent?.headers : opts.headers),
        redirect: choice(
          opts.redirect,
          parent?.redirect ?? "follow",
          ["follow", "error", "manual"],
          "redirect"
        ),
        mode: choice(opts.mode, parent?.mode ?? "cors", ["cors", "same-origin", "no-cors"], "mode"),
        credentials: choice(
          opts.credentials,
          parent?.credentials ?? "same-origin",
          ["omit", "same-origin", "include"],
          "credentials"
        ),
        cache: choice(
          opts.cache,
          parent?.cache ?? "default",
          ["default", "no-store", "reload", "no-cache", "force-cache", "only-if-cached"],
          "cache"
        ),
        referrer: opts.referrer ?? parent?.referrer ?? "about:client",
        referrerPolicy: opts.referrerPolicy ?? parent?.referrerPolicy ?? "",
        integrity: opts.integrity ?? parent?.integrity ?? "",
        keepalive: Boolean(opts.keepalive ?? parent?.keepalive ?? false),
        destination: "",
        duplex: "half",
        isReloadNavigation: false,
        isHistoryNavigation: false
      };
      if (entry.mode === "no-cors") {
        throw host.unsupported("Fetch no-cors guards");
      }
      if (!["default", "no-store"].includes(entry.cache)) {
        throw host.unsupported("Fetch cache/revalidation");
      }
      if (entry.referrer !== "about:client" || entry.referrerPolicy !== "") {
        throw host.unsupported("Fetch referrer policy");
      }
      if (entry.integrity !== "") {
        throw host.unsupported("Fetch integrity verification");
      }
      if (opts.window !== void 0 && opts.window !== null) {
        throw new TypeError("window must be null");
      }
      if (opts.duplex !== void 0 && opts.duplex !== "half") {
        throw new TypeError("duplex must be half");
      }
      const signal = opts.signal === void 0 ? parent?.signal : opts.signal;
      if (signal !== void 0 && signal !== null && !host.abort.isSignal(signal)) {
        throw new TypeError("signal must be an AbortSignal");
      }
      entry.signal = host.abort.AbortSignal.any(signal == null ? [] : [signal]);
      const hasBody = opts.body !== void 0 && opts.body !== null;
      const body = hasBody ? bodies.extract(opts.body) : parent?.body ?? bodies.extract(null);
      if ((method === "GET" || method === "HEAD") && body.stream !== null) {
        throw new TypeError("GET/HEAD cannot have a body");
      }
      if (hasBody && bodies.isStream(opts.body) && opts.duplex === void 0) {
        throw new TypeError("Stream bodies require duplex: half");
      }
      if (entry.keepalive && body.stream !== null && body.source === null) {
        throw new TypeError("keepalive does not support stream bodies");
      }
      entry.body = initializeBody(parent && !hasBody ? bodies.transfer(body) : body, entry.headers);
      requests.set(this, entry);
    }
    clone() {
      const entry = state(requests, this);
      const result = Object.create(Request.prototype);
      requests.set(result, {
        ...entry,
        headers: new Headers(entry.headers),
        signal: host.abort.AbortSignal.any([entry.signal]),
        body: bodies.clone(entry.body)
      });
      return result;
    }
  }
  class Response {
    constructor(body = null, init = void 0) {
      const opts = dictionary(init);
      const status = opts.status === void 0 ? 200 : Math.trunc(Number(opts.status));
      if (!Number.isInteger(status) || status < 200 || status > 599) {
        throw new RangeError("Response status must be 200..599");
      }
      const target = new Headers(opts.headers);
      const extracted = bodies.extract(body);
      if (emptyStatuses.has(status) && extracted.stream !== null) {
        throw new TypeError("This status cannot have a body");
      }
      responses.set(this, {
        status,
        statusText: statusText(opts.statusText),
        headers: target,
        body: initializeBody(extracted, target),
        type: "default",
        url: "",
        redirected: false
      });
    }
    get ok() {
      const { status } = state(responses, this);
      return status >= 200 && status <= 299;
    }
    clone() {
      const entry = state(responses, this);
      const result = Object.create(Response.prototype);
      const target = new Headers(entry.headers);
      if (headers.isImmutable(entry.headers)) {
        headers.immutable(target);
      }
      responses.set(result, { ...entry, headers: target, body: bodies.clone(entry.body) });
      return result;
    }
    static error() {
      const result = Object.create(Response.prototype);
      responses.set(result, {
        status: 0,
        statusText: "",
        headers: headers.immutable(new Headers()),
        body: bodies.extract(null),
        type: "error",
        url: "",
        redirected: false
      });
      return result;
    }
    static redirect(url, status = 302) {
      status = Number(status);
      if (!redirects.has(status)) {
        throw new RangeError("Invalid redirect status");
      }
      const result = new Response(null, {
        status,
        headers: { location: new host.URL(String(url)).href }
      });
      headers.immutable(state(responses, result).headers);
      return result;
    }
    static json(data, init = void 0) {
      const text = JSON.stringify(data);
      if (text === void 0) {
        throw new TypeError("JSON data cannot be undefined");
      }
      const opts = dictionary(init);
      const target = new Headers(opts.headers);
      if (!target.has("content-type")) {
        target.set("content-type", "application/json");
      }
      return new Response(text, { ...opts, headers: target });
    }
  }
  properties(Request, requests, [
    "url",
    "method",
    "headers",
    "redirect",
    "mode",
    "credentials",
    "cache",
    "referrer",
    "referrerPolicy",
    "integrity",
    "keepalive",
    "destination",
    "duplex",
    "signal",
    "isReloadNavigation",
    "isHistoryNavigation"
  ]);
  properties(Response, responses, ["status", "statusText", "headers", "type", "url", "redirected"]);
  return {
    Request,
    Response,
    requestState: (value) => state(requests, value),
    networkResponse(stream, metadata) {
      const result = Object.create(Response.prototype);
      responses.set(result, {
        ...metadata,
        headers: headers.immutable(metadata.headers),
        body: bodies.extract(stream)
      });
      return result;
    }
  };
}

// mite/runtime/bootstrap/fetch/transport.mjs
function createTransport(host, headers, bodies, objects) {
  const { ReadableStream } = host.webStreams;
  const { Headers } = headers;
  const redirectStatuses = /* @__PURE__ */ new Set([301, 302, 303, 307, 308]);
  const badPorts = /* @__PURE__ */ new Set([
    1,
    7,
    9,
    11,
    13,
    15,
    17,
    19,
    20,
    21,
    22,
    23,
    25,
    37,
    42,
    43,
    53,
    69,
    77,
    79,
    87,
    95,
    101,
    102,
    103,
    104,
    109,
    110,
    111,
    113,
    115,
    117,
    119,
    123,
    135,
    137,
    139,
    143,
    161,
    179,
    389,
    427,
    465,
    512,
    513,
    514,
    515,
    526,
    530,
    531,
    532,
    540,
    548,
    554,
    556,
    563,
    587,
    601,
    636,
    989,
    990,
    993,
    995,
    1719,
    1720,
    1723,
    2049,
    3659,
    4045,
    4190,
    5060,
    5061,
    6e3,
    6566,
    6665,
    6666,
    6667,
    6668,
    6669,
    6697,
    10080
  ]);
  function networkError(cause) {
    return new TypeError("fetch failed", { cause });
  }
  function urlFor(value) {
    const url = new host.URL(value);
    if (!["http:", "https:"].includes(url.protocol)) {
      throw networkError(host.unsupported("Fetch URL scheme"));
    }
    if (url.username || url.password) {
      throw new TypeError("Fetch URLs cannot contain credentials");
    }
    if (badPorts.has(Number(url.port))) {
      throw networkError(new Error("Blocked Fetch port"));
    }
    url.hash = "";
    return url;
  }
  function exchange(entry, url, redirected) {
    return new Promise((resolve, reject) => {
      const api = host.require(url.protocol === "https:" ? "node:https" : "node:http");
      const stream = host.require("node:stream");
      let request;
      let incoming;
      let upload;
      let controller;
      let rawReader;
      let finished = false;
      let uploadDone = entry.body.stream === null;
      let responseDone = false;
      let aborted = false;
      let unsubscribe = () => {
      };
      function dispose() {
        unsubscribe();
        unsubscribe = () => {
        };
      }
      function complete() {
        if (uploadDone && responseDone) {
          finished = true;
          dispose();
        }
      }
      function responseComplete() {
        responseDone = true;
        if (!uploadDone && upload) {
          uploadDone = true;
          upload.cancel().catch(() => {
          });
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
        request?.destroy(error instanceof Error ? error : void 0);
        upload?.cancel(reason).catch(() => {
        });
      }
      try {
        const fields = new Headers(entry.headers);
        fields.delete("host");
        if (!fields.has("accept")) {
          fields.set("accept", "*/*");
        }
        if (!fields.has("accept-language")) {
          fields.set("accept-language", "*");
        }
        if (!fields.has("user-agent")) {
          fields.set("user-agent", "mite");
        }
        if (!fields.has("accept-encoding")) {
          fields.set("accept-encoding", "identity");
        }
        if (!fields.has("sec-fetch-mode")) {
          fields.set("sec-fetch-mode", entry.mode);
        }
        if (fields.has("expect") || fields.has("upgrade")) {
          throw host.unsupported("Fetch Expect/Upgrade");
        }
        if (entry.body.length !== null && fields.has("content-length") && fields.get("content-length") !== String(entry.body.length)) {
          throw new TypeError("Body length does not match Content-Length");
        }
        if (entry.body.stream !== null && entry.body.length !== null && !fields.has("content-length")) {
          fields.set("content-length", String(entry.body.length));
        }
        if (entry.body.stream === null && ["POST", "PUT"].includes(entry.method) && !fields.has("content-length")) {
          fields.set("content-length", "0");
        }
        request = api.request(url, { method: entry.method, headers: [...fields].flat() });
        request.on("error", fail);
        request.on("response", (reply) => {
          incoming = reply;
          reply.on("error", () => {
          });
          if (finished) {
            reply.destroy();
            return;
          }
          try {
            const fields2 = new Headers();
            for (let index = 0; index < reply.rawHeaders.length; index += 2) {
              fields2.append(reply.rawHeaders[index], reply.rawHeaders[index + 1]);
            }
            const status = reply.statusCode;
            const location = fields2.get("location");
            if (redirectStatuses.has(status) && location !== null && entry.redirect !== "manual") {
              finished = true;
              dispose();
              request.destroy();
              reply.destroy();
              upload?.cancel().catch(() => {
              });
              if (entry.redirect === "error") {
                reject(networkError(new Error("Redirect mode is error")));
              } else {
                resolve({ location, status });
              }
              return;
            }
            const encoding = fields2.get("content-encoding");
            if (encoding !== null && encoding.toLowerCase() !== "identity") {
              throw host.unsupported("Fetch compressed responses");
            }
            let body = null;
            if (entry.method === "HEAD" || [204, 205, 304].includes(status)) {
              reply.once("end", () => {
                responseComplete();
              });
              reply.resume();
            } else {
              rawReader = stream.Readable.toWeb(reply, {
                strategy: { highWaterMark: 16384, size: (chunk) => chunk.byteLength }
              }).getReader();
              body = new ReadableStream({
                type: "bytes",
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
                  upload?.cancel(reason).catch(() => {
                  });
                  return rawReader.cancel(reason);
                }
              });
            }
            resolve({
              response: objects.networkResponse(body, {
                status,
                statusText: reply.statusMessage ?? "",
                headers: fields2,
                type: "basic",
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
            throw new TypeError("Request body is locked or already used");
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
                  throw new TypeError("Fetch upload chunks must be Uint8Array");
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
  return async function fetch(input, init = void 0) {
    if (arguments.length === 0) {
      throw new TypeError("fetch requires input");
    }
    if (init?.dispatcher !== void 0 || init?.agent !== void 0) {
      throw host.unsupported("Fetch custom dispatcher/agent");
    }
    let entry = objects.requestState(new objects.Request(input, init));
    for (let redirects = 0; ; redirects++) {
      if (entry.signal.aborted) {
        const reason = host.abort.reason(entry.signal);
        entry.body.stream?.cancel(reason).catch(() => {
        });
        throw reason;
      }
      const url = urlFor(entry.url);
      const result = await exchange(entry, url, redirects > 0);
      if (result.response) {
        return result.response;
      }
      if (redirects >= 20) {
        throw networkError(new Error("Too many redirects"));
      }
      const next = urlFor(new host.URL(result.location, url).href);
      if (!new host.URL(result.location, url).hash) {
        next.hash = new host.URL(entry.url).hash;
      }
      const fields = new Headers(entry.headers);
      if (next.origin !== url.origin) {
        for (const name of ["authorization", "proxy-authorization", "cookie"]) {
          fields.delete(name);
        }
      }
      let method = entry.method;
      let body;
      if (result.status === 303 && method !== "GET" && method !== "HEAD" || [301, 302].includes(result.status) && method === "POST") {
        method = "GET";
        body = bodies.extract(null);
        for (const name of [
          "content-length",
          "content-type",
          "content-encoding",
          "content-language",
          "content-location"
        ]) {
          fields.delete(name);
        }
      } else if (entry.body.stream !== null) {
        if (entry.body.source === null) {
          throw networkError(new Error("Cannot replay streamed request body"));
        }
        body = bodies.replay(entry.body);
      } else {
        body = bodies.extract(null);
      }
      entry = { ...entry, url: next.href, method, body, headers: fields };
    }
  };
}

// mite/runtime/bootstrap/fetch/index.mjs
function createFetchGlobals(host) {
  host.unsupported = (name) => Object.assign(new Error(`${name} is not implemented`), { code: "ERR_MITE_UNSUPPORTED_API" });
  const headers = createHeaders();
  const bodies = createBodies(host);
  const objects = createObjects(host, headers, bodies);
  return {
    Headers: headers.Headers,
    Request: objects.Request,
    Response: objects.Response,
    fetch: createTransport(host, headers, bodies, objects)
  };
}
return createFetchGlobals;
})()
