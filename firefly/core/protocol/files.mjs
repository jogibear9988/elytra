/** Queue application callbacks independently of native transport delivery and file bytes. */
export default function createFileHandlers(native, isReady, error, standard) {
  const handlers = new Map();
  const pending = new Map();

  function ready() {
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Protocol handlers require app readiness');
    }
  }

  function scheme(value) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'scheme must be a string');
    }
    if (!/^[a-z][a-z0-9+.-]{0,63}$/.test(value)) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid canonical scheme');
    }
  }

  function metadata(token, response) {
    if (typeof response === 'string') {
      response = { path: response };
    }
    if (response === null || typeof response !== 'object' || Array.isArray(response)) {
      throw error('ERR_INVALID_ARG_TYPE', 'File response must be a path or object');
    }
    for (const key of Reflect.ownKeys(response)) {
      if (!['path', 'headers', 'error'].includes(key)) {
        throw error('ERR_FIREFLY_UNSUPPORTED_API', 'Unsupported file response option');
      }
    }
    if (Object.hasOwn(response, 'error')) {
      if (
        Object.keys(response).length !== 1 ||
        !Number.isInteger(response.error) ||
        response.error >= 0
      ) {
        throw error('ERR_INVALID_ARG_VALUE', 'File response error must be a negative integer');
      }
      return { token, error: true };
    }
    if (
      typeof response.path !== 'string' ||
      response.path.length > 32768 ||
      response.path.includes('\0')
    ) {
      throw error('ERR_INVALID_ARG_VALUE', 'File response needs a bounded native path');
    }
    const headers = [];
    if (response.headers !== undefined) {
      if (
        response.headers === null ||
        typeof response.headers !== 'object' ||
        Array.isArray(response.headers)
      ) {
        throw error('ERR_INVALID_ARG_TYPE', 'File headers must be an object');
      }
      for (const name of Reflect.ownKeys(response.headers)) {
        if (typeof name !== 'string') {
          throw error('ERR_INVALID_ARG_VALUE', 'Header names must be strings');
        }
        const values = response.headers[name];
        for (const value of Array.isArray(values) ? values : [values]) {
          if (typeof value !== 'string') {
            throw error('ERR_INVALID_ARG_TYPE', 'Header values must be strings');
          }
          headers.push([name, value]);
          if (headers.length > 64) {
            throw error('ERR_OUT_OF_RANGE', 'File response header budget exceeded');
          }
        }
      }
    }
    return { token, path: response.path, headers };
  }

  function deny(token) {
    return native.protocolFileReply(JSON.stringify({ token, error: true }));
  }

  function finish(token, response) {
    if (!pending.delete(token)) {
      return false;
    }
    try {
      const wire = JSON.stringify(metadata(token, response));
      if (wire.length > 128 * 1024) {
        throw error('ERR_OUT_OF_RANGE', 'File reply exceeds metadata budget');
      }
      return native.protocolFileReply(wire);
    } catch (failure) {
      deny(token);
      throw failure;
    }
  }

  return {
    api: {
      registerFileProtocol(name, handler) {
        ready();
        scheme(name);
        if (typeof handler !== 'function') {
          throw error('ERR_INVALID_ARG_TYPE', 'File handler must be callable');
        }
        if (!standard(name)) {
          throw error(
            'ERR_FIREFLY_UNSUPPORTED_API',
            'File handlers require a registered standard custom scheme with supportFetchAPI: true'
          );
        }
        if (handlers.has(name)) {
          return false;
        }
        const generation = native.protocolRegisterFile(name);
        if (generation === false) {
          return false;
        }
        handlers.set(name, { handler, generation });
        return true;
      },

      unregisterProtocol(name) {
        ready();
        scheme(name);
        const removed = native.protocolUnregisterFile(name);
        handlers.delete(name);
        for (const [token, request] of pending) {
          if (request.scheme === name) {
            pending.delete(token);
          }
        }
        return removed;
      },

      isProtocolHandled(name) {
        ready();
        scheme(name);
        return handlers.has(name);
      }
    },

    canLoad: (name) => handlers.has(name),

    dispatch(type, request) {
      if (type === 'protocol-cancel') {
        pending.delete(request.token);
        return;
      }
      const entry = handlers.get(request.scheme);
      if (!entry || entry.generation !== request.generation || pending.size >= 64) {
        deny(request.token);
        return;
      }
      pending.set(request.token, { scheme: request.scheme });
      try {
        const result = entry.handler(
          Object.freeze({
            url: request.url,
            method: request.method,
            ...(typeof request.origin === 'string'
              ? { headers: Object.freeze({ origin: request.origin }) }
              : {})
          }),
          (response) => {
            finish(request.token, response);
          }
        );
        // Results never stand in for callbacks, but rejected async handlers must settle refusal.
        Promise.resolve(result).catch(() => {
          if (pending.delete(request.token)) {
            deny(request.token);
          }
        });
      } catch {
        if (pending.delete(request.token)) {
          deny(request.token);
        }
      }
    }
  };
}
