/** Native owned utility children. Main and utility realms exchange bounded binary snapshots. */
export default function createUtilityProcesses(
  native,
  codec,
  EventEmitter,
  Readable,
  app,
  paths,
  childProcessGone
) {
  const owned = new Set();
  let stopped = false;

  function fail(code, message) {
    throw Object.assign(new Error(message), { code });
  }

  function unsupported(name) {
    fail('ERR_FIREFLY_UNSUPPORTED_API', `${name} is not implemented`);
  }

  function text(value, name) {
    if (typeof value !== 'string' || value.includes('\0')) {
      fail('ERR_INVALID_ARG_TYPE', `${name} must be a string without NUL`);
    }
    return value;
  }

  function strings(value, name) {
    if (!Array.isArray(value) || value.length > 1024) {
      fail('ERR_INVALID_ARG_TYPE', `${name} must be a bounded string array`);
    }
    return value.map((item) => text(item, name));
  }

  function modes(value = 'inherit') {
    const result = typeof value === 'string' ? ['ignore', value, value] : value;
    if (!Array.isArray(result) || result.length !== 3 || result[0] !== 'ignore') {
      unsupported('utilityProcess stdin/additional descriptors');
    }
    for (const mode of result) {
      if (!['ignore', 'inherit', 'pipe'].includes(mode)) {
        unsupported(`utilityProcess stdio ${mode}`);
      }
    }
    return result.slice();
  }

  function configuration(modulePath, args, options) {
    text(modulePath, 'modulePath');
    if (!modulePath) {
      fail('ERR_INVALID_ARG_VALUE', 'modulePath must not be empty');
    }
    if (!options || typeof options !== 'object' || Array.isArray(options)) {
      fail('ERR_INVALID_ARG_TYPE', 'Utility options must be an object');
    }
    const supported = new Set([
      'env',
      'cwd',
      'stdio',
      'execArgv',
      'serviceName',
      'allowLoadingUnsignedLibraries',
      'respondToAuthRequestsFromMainProcess'
    ]);
    for (const key of Object.keys(options)) {
      if (!supported.has(key)) {
        unsupported(`utilityProcess option ${key}`);
      }
    }
    if (options.execArgv !== undefined && strings(options.execArgv, 'execArgv').length) {
      unsupported('utilityProcess engine command-line flags');
    }
    for (const key of ['allowLoadingUnsignedLibraries', 'respondToAuthRequestsFromMainProcess']) {
      if (options[key] !== undefined && options[key] !== false) {
        unsupported(`utilityProcess ${key}`);
      }
    }
    const serviceName =
      options.serviceName === undefined
        ? 'Node Utility Process'
        : text(options.serviceName, 'serviceName');
    if (new TextEncoder().encode(serviceName).length > 4096) {
      fail('ERR_OUT_OF_RANGE', 'serviceName exceeds its UTF-8 byte limit');
    }
    const env = options.env ?? process.env;
    if (!env || typeof env !== 'object' || Array.isArray(env)) {
      fail('ERR_INVALID_ARG_TYPE', 'env must be an object');
    }
    const environment = [];
    for (const key of Object.keys(env)) {
      text(key, 'environment key');
      if (!key || key.includes('=')) {
        fail('ERR_INVALID_ARG_VALUE', 'Invalid environment key');
      }
      if (env[key] !== undefined) {
        environment.push(`${key}=${text(env[key], 'environment value')}`);
      }
    }
    return {
      file: process.execPath,
      entry: paths.resolve(modulePath),
      args: strings(args, 'args'),
      cwd: options.cwd === undefined ? '' : text(options.cwd, 'cwd'),
      env: environment,
      stdio: modes(options.stdio),
      serviceName: serviceName || 'Node Utility Process'
    };
  }

  /** Keep the stream pressure bound separate from the native framing channel. */
  function output() {
    return new Readable({ read() {} });
  }

  const authority = new WeakMap();
  const token = {};

  class UtilityProcess extends EventEmitter {
    constructor(key, spec) {
      super();
      if (key !== token) {
        unsupported('Direct UtilityProcess construction');
      }
      const state = {
        id: 0,
        pid: undefined,
        exited: false,
        killed: false,
        stdout: null,
        stderr: null
      };
      authority.set(this, state);
      state.stdout = spec.stdio[1] === 'pipe' ? output() : null;
      state.stderr = spec.stdio[2] === 'pipe' ? output() : null;
      owned.add(this);
      let result;
      try {
        result = native.spawn(spec, (kind, id, value) => {
          if (stopped || id !== state.id) {
            return;
          }
          if (kind === 'spawn') {
            state.pid = value;
            this.emit('spawn');
          } else if (kind === 'stdout' || kind === 'stderr') {
            const stream = state[kind];
            if (
              stream &&
              !stream.destroyed &&
              !stream.push(value) &&
              stream.readableLength > 1048576
            ) {
              stream.destroy();
              native.kill(state.id);
              this.emit(
                'error',
                Object.assign(new Error('Utility output exceeds its queue budget'), {
                  code: 'ERR_FIREFLY_UTILITY_LIMIT'
                })
              );
            }
          } else if (kind === 'stdoutEnd' || kind === 'stderrEnd') {
            const name = kind === 'stdoutEnd' ? 'stdout' : 'stderr';
            state[name]?.push(null);
            state[name] = null;
            if (state.exited && !state.stdout && !state.stderr) {
              owned.delete(this);
            }
          } else if (kind === 'data') {
            this.emit('message', codec.decodeIpc(value));
          } else if (kind === 'gone') {
            const details = JSON.parse(value);
            if (
              details.type !== 'Utility' ||
              details.name !== spec.serviceName ||
              details.pid !== state.pid ||
              !Number.isInteger(details.exitCode) ||
              !['abnormal-exit', 'crashed', 'killed'].includes(details.reason)
            ) {
              fail('ERR_FIREFLY_UTILITY_CHANNEL', 'Invalid native utility exit details');
            }
            childProcessGone?.(details);
          } else if (kind === 'exit') {
            state.exited = true;
            state.pid = undefined;
            this.emit('exit', value);
            // Keep streams owned until their separate EOF notifications.
            if (!state.stdout && !state.stderr) {
              owned.delete(this);
            }
          } else if (kind === 'error') {
            this.emit(
              'error',
              Object.assign(new Error(String(value)), {
                code: 'ERR_FIREFLY_UTILITY_CHANNEL'
              })
            );
          }
        });
      } catch (failure) {
        owned.delete(this);
        throw failure;
      }
      state.id = result.id;
      if (result.status) {
        owned.delete(this);
        state.exited = true;
        state.stdout?.destroy();
        state.stderr?.destroy();
        state.stdout = state.stderr = null;
        fail('ERR_FIREFLY_UTILITY_SPAWN', `Utility spawn failed (${result.status})`);
      }
    }

    get pid() {
      return authority.get(this).pid;
    }

    get stdout() {
      return authority.get(this).stdout;
    }

    get stderr() {
      return authority.get(this).stderr;
    }

    postMessage(message, transfer) {
      const state = authority.get(this);
      if (stopped || state.exited) {
        return;
      }
      if (transfer !== undefined && (!Array.isArray(transfer) || transfer.length)) {
        unsupported('Utility native MessagePort transfer');
      }
      const wire = codec.encodeIpc(message);
      const status = native.send(state.id, wire);
      if (status) {
        fail('ERR_FIREFLY_UTILITY_CHANNEL', `Utility channel send failed (${status})`);
      }
    }

    kill() {
      const state = authority.get(this);
      if (stopped || state.exited || state.killed) {
        return false;
      }
      state.killed = native.kill(state.id);
      return state.killed;
    }
  }

  function fork(modulePath, args = [], options = {}) {
    if (!app.isReady()) {
      fail('ERR_FIREFLY_APP_NOT_READY', 'utilityProcess.fork requires app ready');
    }
    if (stopped) {
      fail('ERR_FIREFLY_APP_EXITED', 'Application has exited');
    }
    if (!native || !Readable) {
      unsupported('Native utility processes in this host');
    }
    return new UtilityProcess(token, configuration(modulePath, args, options));
  }

  function shutdown() {
    if (stopped) {
      return;
    }
    stopped = true;
    native?.shutdown();
    for (const child of owned) {
      const state = authority.get(child);
      state.exited = true;
      state.pid = undefined;
      state.stdout?.destroy();
      state.stderr?.destroy();
      state.stdout = state.stderr = null;
    }
    owned.clear();
  }

  return { utilityProcess: { fork }, shutdown };
}
