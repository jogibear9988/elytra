/** Non-reusing connection admission. Actual sockets, rather than public bookkeeping, return credit. */

/** Node-compatible origin keys; HTTPS may borrow this without an HTTPS constructor brand. */
export function getAgentName(options = {}, secure = false) {
  let key = `${options.host || 'localhost'}:${options.port || ''}:${options.localAddress || ''}`;
  if (options.family === 4 || options.family === 6) {
    key += `:${options.family}`;
  }
  if (options.socketPath) {
    key += `:${options.socketPath}`;
  }
  if (!secure) {
    return key;
  }
  // These values distinguish TLS policy even while connection reuse is disabled.
  for (const field of [
    'ca',
    'cert',
    'clientCertEngine',
    'ciphers',
    'key',
    'pfx',
    'rejectUnauthorized',
    'servername',
    'minVersion',
    'maxVersion',
    'secureProtocol',
    'crl',
    'honorCipherOrder',
    'ecdhCurve',
    'dhparam',
    'secureOptions',
    'sessionIdContext',
    'sigalgs',
    'privateKeyIdentifier',
    'privateKeyEngine'
  ]) {
    const value = options[field];
    const present = ['rejectUnauthorized', 'honorCipherOrder', 'secureOptions'].includes(field)
      ? value !== undefined
      : !!value;
    key += ':';
    if (present && !(field === 'servername' && value === options.host)) {
      key += field === 'sigalgs' ? JSON.stringify(value) : String(value);
    }
  }
  return key;
}

export default function createHttpAgent(host, protocol, defaultConnect) {
  const { EventEmitter, net, process } = host;
  const tick = process.nextTick.bind(process);
  const Socket = net.Socket;
  const states = new WeakMap();
  const socketOwners = new WeakMap();

  function error(code, message, Type = Error) {
    return Object.assign(new Type(message), { code });
  }

  function unsupported(message) {
    throw error('ERR_MITE_UNSUPPORTED_API', message);
  }

  function stateFor(agent) {
    const state = states.get(agent);
    if (!state) {
      throw error('ERR_INVALID_THIS', 'Invalid Agent receiver', TypeError);
    }
    return state;
  }

  function maximum(value, name) {
    if (
      value !== Infinity &&
      (typeof value !== 'number' || !Number.isInteger(value) || value < 1)
    ) {
      throw error('ERR_OUT_OF_RANGE', `${name} must be a positive integer or Infinity`, RangeError);
    }
    return value;
  }

  function removePublic(agent, table, name, value) {
    const list = agent[table][name];
    if (!Array.isArray(list)) {
      return;
    }
    const index = list.indexOf(value);
    if (index !== -1) {
      list.splice(index, 1);
    }
    if (!list.length) {
      delete agent[table][name];
    }
  }

  function destroyExtra(socket, entry) {
    // A repeated callback must never tear down the socket already assigned to
    // this request, or a socket owned by another admitted request.
    if (
      socket instanceof Socket &&
      socket !== entry.socket &&
      socket !== entry.delegated &&
      !socketOwners.has(socket) &&
      !socket.destroyed
    ) {
      socket.destroy();
    }
  }

  function schedule(agent, state) {
    if (state.scheduled || state.destroying) {
      return;
    }
    state.scheduled = true;

    tick(() => {
      state.scheduled = false;
      pump(agent, state);
    });
  }

  function release(agent, state, entry) {
    if (entry.phase === 'released') {
      return;
    }
    const queued = entry.phase === 'queued';
    entry.phase = 'released';
    state.entries.delete(entry);
    state.byRequest.delete(entry.req);
    const index = state.queue.indexOf(entry);
    if (index !== -1) {
      state.queue.splice(index, 1);
    }
    if (queued) {
      removePublic(agent, 'requests', entry.name, entry.req);
    }
    if (entry.socket) {
      removePublic(agent, 'sockets', entry.name, entry.socket);
      if (socketOwners.get(entry.socket) === entry) {
        socketOwners.delete(entry.socket);
      }
      agent.totalSocketCount--;
      entry.socket.removeListener('close', entry.socketClose);
      entry.socket.removeListener('agentRemove', entry.agentRemove);
    }
    entry.req.removeListener('close', entry.requestClose);
    entry.req.removeListener('socket', entry.requestSocket);
    schedule(agent, state);
  }

  function record(agent, state, req, options) {
    const entry = {
      req,
      options,
      name: agent.getName(options),
      phase: 'queued',
      socket: null,
      delegated: null,
      callbackDone: false
    };

    entry.requestClose = () => {
      if (entry.phase === 'socket') {
        // The native close, not request.destroy(), returns connection credit.
        if (!entry.socket.destroyed) {
          entry.socket.destroy();
        }
      } else {
        release(agent, state, entry);
      }
    };

    entry.requestSocket = (socket) => {
      if (entry.phase === 'connecting' && entry.socket !== socket) {
        // agent-base may resolve connect() to another Agent and never call our
        // createSocket callback. Its actual socket event retires only our slot.
        entry.delegated = socket;
        release(agent, state, entry);
      }
    };

    state.entries.add(entry);
    state.byRequest.set(req, entry);
    req.once('close', entry.requestClose);
    req.on('socket', entry.requestSocket);
    return entry;
  }

  function available(agent, state, name) {
    const perOrigin = maximum(agent.maxSockets, 'maxSockets');
    const total = maximum(agent.maxTotalSockets, 'maxTotalSockets');
    let originCount = 0;
    let count = 0;
    for (const entry of state.entries) {
      if (entry.phase !== 'connecting' && entry.phase !== 'socket') {
        continue;
      }
      count++;
      if (entry.name === name) {
        originCount++;
      }
    }
    return count < total && originCount < perOrigin;
  }

  function ownSocket(agent, state, entry, socket) {
    if (!(socket instanceof Socket) || socket.destroyed) {
      throw error(
        'ERR_INVALID_RETURN_VALUE',
        'Agent connector must return a live net.Socket',
        TypeError
      );
    }
    if (entry.socket === socket) {
      return;
    }
    if (socketOwners.has(socket)) {
      throw error('ERR_INVALID_ARG_VALUE', 'Socket already belongs to a request');
    }
    if (entry.phase === 'released' || entry.req.destroyed || state.destroying) {
      destroyExtra(socket, entry);
      throw error('ECONNRESET', 'Request closed before socket admission');
    }
    entry.socket = socket;
    entry.phase = 'socket';
    socketOwners.set(socket, entry);
    agent.sockets[entry.name] ??= [];
    agent.sockets[entry.name].push(socket);
    agent.totalSocketCount++;

    entry.socketClose = () => release(agent, state, entry);

    entry.agentRemove = () => release(agent, state, entry);

    socket.once('close', entry.socketClose);
    socket.once('agentRemove', entry.agentRemove);
  }

  function dispatch(agent, state, entry) {
    entry.phase = 'connecting';
    entry.dispatched = true;
    removePublic(agent, 'requests', entry.name, entry.req);

    const done = (failure, socket) => {
      if (entry.callbackDone) {
        destroyExtra(socket, entry);
        return;
      }
      entry.callbackDone = true;
      if (entry.phase === 'released' || entry.req.destroyed) {
        destroyExtra(socket, entry);
        return;
      }
      try {
        if (!failure) {
          ownSocket(agent, state, entry, socket);
        }
      } catch (err) {
        failure = err;
      }
      if (failure) {
        destroyExtra(socket, entry);
        release(agent, state, entry);
        entry.req.onSocket(null, failure);
      } else {
        entry.req.onSocket(socket);
      }
    };

    try {
      agent.createSocket(entry.req, entry.options, done);
    } catch (failure) {
      done(failure);
    }
  }

  function pump(agent, state) {
    if (state.pumping || state.destroying) {
      return;
    }
    state.pumping = true;
    try {
      // Skip a saturated origin while retaining FIFO within every origin.
      for (let index = 0; index < state.queue.length; ) {
        const entry = state.queue[index];
        if (entry.req.destroyed) {
          release(agent, state, entry);
          continue;
        }
        let allowed;
        try {
          allowed = available(agent, state, entry.name);
        } catch (failure) {
          release(agent, state, entry);
          entry.req.onSocket(null, failure);
          continue;
        }
        if (!allowed) {
          index++;
          continue;
        }
        state.queue.splice(index, 1);
        dispatch(agent, state, entry);
      }
    } finally {
      state.pumping = false;
    }
  }

  class Agent extends EventEmitter {
    constructor(options = {}) {
      super();
      if (!options || typeof options !== 'object' || Array.isArray(options)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Agent options must be an object', TypeError);
      }
      if (options.keepAlive === true) {
        unsupported('Agent keep-alive/reuse is not implemented');
      }
      if (options.keepAlive !== undefined && typeof options.keepAlive !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'keepAlive must be boolean', TypeError);
      }
      if (options.proxyEnv !== undefined) {
        unsupported('Agent proxyEnv is not implemented');
      }
      this.options = { ...options, path: null };
      this.options.noDelay ??= true;
      this.defaultPort = options.defaultPort ?? (protocol === 'https:' ? 443 : 80);
      this.protocol = options.protocol ?? protocol;
      this.requests = Object.create(null);
      this.sockets = Object.create(null);
      this.freeSockets = Object.create(null);
      this.keepAlive = false;
      this.keepAliveMsecs = options.keepAliveMsecs ?? 1000;
      this.maxSockets = maximum(options.maxSockets ?? Agent.defaultMaxSockets, 'maxSockets');
      this.maxTotalSockets = maximum(options.maxTotalSockets ?? Infinity, 'maxTotalSockets');
      this.maxFreeSockets = options.maxFreeSockets ?? 256;
      this.totalSocketCount = 0;
      this.scheduling = options.scheduling ?? 'lifo';
      if (!['lifo', 'fifo'].includes(this.scheduling)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid Agent scheduling');
      }
      states.set(this, {
        entries: new Set(),
        byRequest: new WeakMap(),
        queue: [],
        scheduled: false,
        pumping: false,
        destroying: false
      });
    }

    getName(options = {}) {
      return getAgentName(options, protocol === 'https:');
    }

    createConnection(options, callback) {
      stateFor(this);
      return defaultConnect(options, callback);
    }

    createSocket(req, options, callback) {
      const state = stateFor(this);
      if (typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'callback must be a function', TypeError);
      }
      let entry = state.byRequest.get(req);
      if (!entry) {
        const merged = { ...options, ...this.options };
        entry = record(this, state, req, merged);
        if (!available(this, state, entry.name)) {
          release(this, state, entry);

          tick(() => callback(error('ERR_SOCKET_CANNOT_SEND', 'Agent connection limit reached')));

          return;
        }
        entry.phase = 'connecting';
      }
      let completed = false;
      let accepted;

      const finish = (failure, socket) => {
        if (completed) {
          if (socket !== accepted) {
            destroyExtra(socket, entry);
          }
          return;
        }
        completed = true;
        if (entry.phase === 'released' || req.destroyed) {
          destroyExtra(socket, entry);
          return;
        }
        try {
          if (!failure) {
            ownSocket(this, state, entry, socket);
          }
        } catch (err) {
          failure = err;
        }
        if (failure) {
          destroyExtra(socket, entry);
          if (!entry.dispatched) {
            release(this, state, entry);
          }
        } else {
          accepted = socket;
        }
        callback(failure, socket);
      };

      try {
        const socket = this.createConnection({ ...entry.options, ...this.options }, finish);
        if (socket !== undefined && socket !== null) {
          finish(null, socket);
        }
      } catch (failure) {
        finish(failure);
      }
    }

    addRequest(req, options, port, localAddress) {
      const state = stateFor(this);
      if (!req || typeof req.onSocket !== 'function' || typeof req.once !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'request must be a ClientRequest', TypeError);
      }
      if (req.destroyed) {
        return;
      }
      // This slice owns one connection per response, including finite-limit
      // Agents. A free socket is never advertised as reusable.
      req.shouldKeepAlive = false;
      if (state.destroying) {
        req.onSocket(null, error('ECONNRESET', 'Agent is being destroyed'));
        return;
      }
      if (this.keepAlive) {
        unsupported('Agent keep-alive/reuse is not implemented');
      }
      if (state.byRequest.has(req)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Request already admitted by this Agent');
      }
      if (typeof options === 'string') {
        options = { host: options, port, localAddress };
      }
      const merged = { ...options, ...this.options };
      const entry = record(this, state, req, merged);
      this.requests[entry.name] ??= [];
      this.requests[entry.name].push(req);
      state.queue.push(entry);
      pump(this, state);
    }

    destroy() {
      const state = stateFor(this);
      state.destroying = true;
      try {
        for (const entry of [...state.entries]) {
          if (entry.socket) {
            if (!entry.socket.destroyed) {
              entry.socket.destroy();
            }
          } else {
            release(this, state, entry);
            entry.req.onSocket(null, error('ECONNRESET', 'Agent connection destroyed'));
          }
        }
      } finally {
        state.destroying = false;
      }
    }

    keepSocketAlive() {
      stateFor(this);
      return false;
    }

    reuseSocket() {
      stateFor(this);
      unsupported('Agent socket reuse is not implemented');
    }
  }
  Agent.defaultMaxSockets = Infinity;
  return { Agent, globalAgent: new Agent() };
}
