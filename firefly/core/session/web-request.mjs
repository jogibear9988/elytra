/**
 * Session request decisions. Native events carry authenticated document identity;
 * main-process net calls enter the same policy before each real transport hop.
 * A callback describes a decision only: native code retains lifetime authority.
 */
import requestFilter from './request-filter.mjs';

export default function createWebRequest(
  native,
  isReady,
  isStopped,
  contentsFromId,
  frameFromDocument,
  error,
  unsupported,
  abortHooks
) {
  const maximum = 0x7fffffff;
  const handlers = { before: null, sendHeaders: null, headers: null };
  const pending = new Set();
  const resources = new Set([
    'mainFrame',
    'subFrame',
    'stylesheet',
    'script',
    'image',
    'font',
    'object',
    'xhr',
    'ping',
    'cspReport',
    'media',
    'webSocket',
    'other'
  ]);
  let epoch = 0;
  let stopped = false;
  let configuring = false;
  let healthy = true;
  let sequence = 0;

  const id = (value) => Number.isInteger(value) && value > 0 && value <= maximum;

  const active = () => !stopped && !isStopped();

  function headers(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'responseHeaders must be a header record');
    }
    const entries = Object.entries(value);
    if (entries.length > 128) {
      throw error('ERR_OUT_OF_RANGE', 'Too many response headers');
    }
    let size = 0;
    const names = new Set();
    const result = Object.create(null);
    for (const [name, field] of entries) {
      if (!/^[!#$%&'*+.^_`|~0-9a-z-]+$/i.test(name)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid response header name');
      }
      if (names.has(name.toLowerCase())) {
        throw error('ERR_INVALID_ARG_VALUE', 'Duplicate case-insensitive response header');
      }
      names.add(name.toLowerCase());
      const values = Array.isArray(field) ? [...field] : [field];
      if (
        values.length > 128 ||
        values.some((item) => typeof item !== 'string' || /[^\t\x20-\x7e\x80-\xff]/.test(item))
      ) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid response header value');
      }
      size += name.length + values.reduce((sum, item) => sum + item.length + 2, 0);
      if (size > 32768) {
        throw error('ERR_OUT_OF_RANGE', 'Response header budget exceeded');
      }
      result[name] = values;
    }
    return result;
  }

  function decision(stage, value, originalHeaders) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'webRequest callback requires a decision object');
    }
    for (const key of Object.keys(value)) {
      if (
        key !== 'cancel' &&
        !(
          (stage === 'headers' && key === 'responseHeaders') ||
          (stage === 'sendHeaders' && key === 'requestHeaders')
        )
      ) {
        unsupported('webRequest decision ' + key);
      }
    }
    if (value.cancel !== undefined && typeof value.cancel !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', 'webRequest cancel must be a boolean');
    }
    const result = { cancel: value.cancel === true };
    const field = stage === 'sendHeaders' ? 'requestHeaders' : 'responseHeaders';
    if (!result.cancel && value[field] !== undefined) {
      result[field] = headers(value[field]);
      for (const name of [
        ...(stage === 'sendHeaders' ? ['host'] : []),
        'content-length',
        'transfer-encoding',
        'connection',
        'trailer',
        'upgrade',
        'keep-alive',
        'proxy-connection',
        'te'
      ]) {
        const values = (record) =>
          Object.entries(record)
            .filter(([key]) => key.toLowerCase() === name)
            .flatMap(([, items]) => items);

        if (JSON.stringify(values(result[field])) !== JSON.stringify(values(originalHeaders))) {
          unsupported('webRequest response framing header ' + name);
        }
      }
    }
    return result;
  }

  function metadata(data) {
    if (
      !id(data.id) ||
      typeof data.url !== 'string' ||
      data.url.length > 65536 ||
      typeof data.method !== 'string' ||
      !/^[!#$%&'*+.^_`|~0-9a-z-]{1,32}$/i.test(data.method) ||
      !resources.has(data.resourceType) ||
      typeof data.referrer !== 'string' ||
      data.referrer.length > 65536
    ) {
      throw error('ERR_FIREFLY_WEB_REQUEST_DATA', 'Invalid native request metadata');
    }
    const result = {
      id: data.id,
      url: data.url,
      method: data.method,
      resourceType: data.resourceType,
      referrer: data.referrer,
      timestamp: Date.now(),
      frame: null
    };
    if (data.initiatorOrigin !== undefined) {
      if (typeof data.initiatorOrigin !== 'string' || data.initiatorOrigin.length > 65536) {
        throw error('ERR_FIREFLY_WEB_REQUEST_DATA', 'Invalid request initiator');
      }
      result.initiatorOrigin = data.initiatorOrigin;
    }
    if (data.contentsId !== undefined) {
      const owner = id(data.contentsId) && contentsFromId(data.contentsId);
      if (!owner || owner.isDestroyed()) {
        throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Request owner is no longer active');
      }
      result.webContentsId = data.contentsId;
      result.webContents = owner;
      if (data.document !== undefined && data.document !== null && data.document !== '') {
        result.frame = frameFromDocument(data.contentsId, data.document);
        if (!result.frame || result.frame.isDestroyed()) {
          throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Request document is no longer active');
        }
      }
    }
    if (data.stage === 'sendHeaders') {
      const original = headers(data.requestHeaders);
      result.requestHeaders = Object.fromEntries(
        Object.entries(original).map(([name, values]) => [name, values.join(', ')])
      );
    }
    if (data.stage === 'headers') {
      if (
        !Number.isInteger(data.statusCode) ||
        data.statusCode < 100 ||
        data.statusCode > 599 ||
        (data.statusLine !== undefined &&
          (typeof data.statusLine !== 'string' ||
            data.statusLine.length > 8192 ||
            /[\r\n\0]/.test(data.statusLine)))
      ) {
        throw error('ERR_FIREFLY_WEB_REQUEST_DATA', 'Invalid response status');
      }
      result.statusCode = data.statusCode;
      if (data.statusLine !== undefined) {
        result.statusLine = data.statusLine;
      }
      result.responseHeaders = headers(data.responseHeaders);
    }
    return result;
  }

  function invalidate() {
    for (const finish of [...pending]) {
      finish({ cancel: true });
    }
  }

  function register(stage, args) {
    if (!active()) {
      throw error('ERR_FIREFLY_SESSION_CLOSED', 'Session request policy is closed');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'webRequest requires app readiness');
    }
    if (configuring) {
      throw error('ERR_FIREFLY_SESSION_REENTRANT', 'Request configuration is already in progress');
    }
    if (args.length < 1 || args.length > 2) {
      throw error('ERR_INVALID_ARG_TYPE', 'Expected an optional filter and a listener');
    }
    const listener = args[args.length - 1];
    if (listener !== null && typeof listener !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'webRequest listener must be a function or null');
    }
    const match = requestFilter(args.length === 2 ? args[0] : undefined, error, unsupported);
    if (
      typeof native.webRequest?.configure !== 'function' ||
      typeof native.webRequest?.complete !== 'function'
    ) {
      unsupported('Session native request interception');
    }
    if (epoch === maximum) {
      throw error('ERR_OUT_OF_RANGE', 'Request policy generation exhausted');
    }
    const next = { ...handlers, [stage]: listener === null ? null : { listener, match } };
    // Utility processes currently accept no session/partition. Electron gives
    // these children their own system network context; they are not clients of
    // defaultSession interception. Main/renderer policy remains independent.
    ++epoch;
    configuring = true;
    try {
      invalidate();
      native.webRequest.configure(
        epoch,
        next.before !== null || next.sendHeaders !== null,
        next.headers !== null
      );
      if (!active()) {
        throw error('ERR_FIREFLY_SESSION_CLOSED', 'Session closed during request configuration');
      }
      Object.assign(handlers, next);
      healthy = true;
    } catch (failure) {
      handlers.before = handlers.sendHeaders = handlers.headers = null;
      healthy = false;
      // A failed native configuration poisons only this session policy until
      // an explicit successful configuration repairs its authenticated routing.
      throw failure;
    } finally {
      configuring = false;
    }
  }

  function run(stage, data, complete, signal) {
    let done = false;
    let timer;
    let unsubscribe;
    const current = epoch;
    const selected = handlers[stage];
    const deadline = Date.now() + 30000;

    function finish(value) {
      if (done) {
        return;
      }
      done = true;
      pending.delete(finish);
      clearTimeout(timer);
      unsubscribe?.();
      complete(value);
    }

    if (
      !active() ||
      !healthy ||
      configuring ||
      data.epoch !== current ||
      !['before', 'sendHeaders', 'headers'].includes(stage)
    ) {
      finish({ cancel: true });
      return;
    }
    let details;
    try {
      details = metadata({ ...data, stage });
    } catch {
      finish({ cancel: true });
      return;
    }
    if (!selected || !selected.match(details)) {
      finish({ cancel: false });
      return;
    }
    // Applications commonly mutate details.responseHeaders in place. Keep a
    // private original for framing validation before handing them the record.
    const originalHeaders =
      stage === 'headers'
        ? headers(details.responseHeaders)
        : stage === 'sendHeaders'
          ? headers(data.requestHeaders)
          : undefined;
    if (pending.size >= 64) {
      finish({ cancel: true });
      return;
    }
    pending.add(finish);
    timer = setTimeout(() => finish({ cancel: true }), 30000);
    if (signal && abortHooks) {
      const detach = abortHooks.subscribe(signal, () => finish({ cancel: true }));
      if (done) {
        detach?.();
      } else {
        unsubscribe = detach;
      }
    }
    if (done) {
      return;
    }

    const callback = (value) => {
      if (done) {
        return;
      }
      if (
        !active() ||
        Date.now() >= deadline ||
        current !== epoch ||
        handlers[stage] !== selected ||
        details.frame?.isDestroyed() ||
        details.webContents?.isDestroyed()
      ) {
        finish({ cancel: true });
        return;
      }
      try {
        const result = decision(stage, value, originalHeaders);
        // Decision fields can be accessors supplied by trusted application code;
        // their nested work must not resurrect an obsolete native continuation.
        finish(
          active() &&
            current === epoch &&
            handlers[stage] === selected &&
            Date.now() < deadline &&
            !details.frame?.isDestroyed() &&
            !details.webContents?.isDestroyed()
            ? result
            : { cancel: true }
        );
      } catch (failure) {
        finish({ cancel: true });
        throw failure;
      }
    };

    try {
      selected.listener(details, callback);
    } catch (failure) {
      finish({ cancel: true });
      throw failure;
    }
  }

  function runStages(stage, data, complete, signal) {
    if (stage !== 'before') {
      run(stage, data, complete, signal);
      return;
    }
    // Both callbacks run while the actual native request is suspended, before
    // preflight/transport. This is not a snapshot of later automatic Cookie/Host.
    run(
      'before',
      data,
      (result) => {
        if (result.cancel || !handlers.sendHeaders) {
          complete(result);
          return;
        }
        run('sendHeaders', data, complete, signal);
      },
      signal
    );
  }

  function dispatch(data) {
    if (!data || !id(data.token) || !id(data.epoch)) {
      return;
    }

    runStages(data.stage, data, (value) =>
      native.webRequest.complete(data.token, data.epoch, JSON.stringify(value))
    );
  }

  function intercept(stage, data, signal) {
    return new Promise((resolve, reject) => {
      runStages(
        stage,
        { ...data, epoch },
        (value) => {
          if (value.cancel) {
            reject(error('ERR_BLOCKED_BY_CLIENT', 'Request cancelled by session policy'));
          } else {
            resolve(value);
          }
        },
        signal
      );
    });
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    handlers.before = handlers.sendHeaders = handlers.headers = null;
    invalidate();
    if (epoch && epoch < maximum) {
      native.webRequest.configure(++epoch, false, false);
    }
  }

  const api = {
    onBeforeRequest(...args) {
      register('before', args);
    },

    onBeforeSendHeaders(...args) {
      register('sendHeaders', args);
    },

    onHeadersReceived(...args) {
      register('headers', args);
    }
  };
  for (const name of [
    'onSendHeaders',
    'onResponseStarted',
    'onBeforeRedirect',
    'onCompleted',
    'onErrorOccurred'
  ]) {
    api[name] = () => unsupported('webRequest.' + name);
  }
  return {
    api: Object.freeze(api),
    dispatch,
    intercept,
    stop,

    allocateId() {
      if (sequence >= maximum - 1) {
        throw error('ERR_OUT_OF_RANGE', 'Network request identity exhausted');
      }
      return (sequence += 2);
    }
  };
}
