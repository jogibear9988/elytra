/** Share native stat polling by resolved path; handle ownership remains explicit even when unrefed. */
export default function createStatWatchers(host, helpers) {
  const { native, EventEmitter } = host;
  const { path, nativePath, stats } = helpers;
  const watchers = new Map();
  const states = new WeakMap();
  const tick = host.process.nextTick;
  const token = {};

  function invalid(code, message, Type = TypeError) {
    return Object.assign(new Type(message), { code });
  }

  function state(value) {
    const result = states.get(value);
    if (!result) {
      throw invalid('ERR_INVALID_ARG_TYPE', 'Illegal StatWatcher receiver');
    }
    return result;
  }

  function filename(value) {
    // Node 24 resolves validated paths before option handling. Its path.resolve rejects Buffers.
    return host.path.resolve(path(value));
  }

  class StatWatcher extends EventEmitter {
    constructor(key, name, bigint) {
      super();
      if (key !== token) {
        throw invalid('ERR_ILLEGAL_CONSTRUCTOR', 'StatWatcher cannot be constructed directly');
      }
      states.set(this, {
        name,
        bigint,
        id: null,
        stopped: false,
        admitted: false,
        early: [],
        refs: 1,
        maximum: 1
      });
    }

    ref() {
      const current = state(this);
      if (current.refs === current.maximum) {
        return this;
      }
      if (current.id !== null && current.refs++ === 0) {
        native.statWatchRef(current.id, true);
      }
      return this;
    }

    unref() {
      const current = state(this);
      if (current.refs === 0) {
        return this;
      }
      if (current.id !== null && --current.refs === 0) {
        native.statWatchRef(current.id, false);
      }
      return this;
    }

    stop() {
      const current = state(this);
      if (current.stopped) {
        return;
      }
      current.stopped = true;
      current.early.length = 0;
      const id = current.id;
      current.id = null;
      if (id !== null) {
        native.statWatchStop(id);
      }

      tick(() => this.emit('stop'));
    }
  }

  function notify(watcher, status, currentRaw, previousRaw) {
    const current = state(watcher);
    if (current.stopped) {
      return;
    }
    // Missing and inaccessible paths are change snapshots, including epoch/zero current stats.
    const value = [stats(currentRaw, current.bigint), stats(previousRaw, current.bigint)];
    if (!current.admitted) {
      if (current.early.length >= 64) {
        watcher.stop();
        throw invalid('ERR_MITE_STAT_WATCH_LIMIT', 'Stat watcher admission queue exhausted', Error);
      }
      current.early.push(value);
      return;
    }
    watcher.emit('change', ...value);
  }

  function schedule(watcher) {
    tick(() => {
      const current = state(watcher);
      while (!current.stopped && current.early.length) {
        watcher.emit('change', ...current.early.shift());
      }
    });
  }

  function watchFile(value, options, listener) {
    const name = filename(value);
    if (options === null || typeof options !== 'object') {
      listener = options;
      options = null;
    }
    const selected = { interval: 5007, persistent: true, ...options };
    if (typeof listener !== 'function') {
      throw invalid('ERR_INVALID_ARG_TYPE', 'watchFile listener must be a function');
    }
    let watcher = watchers.get(name);
    if (watcher) {
      const current = state(watcher);
      // As in Node, later options are ignored and sharing adds one logical reference.
      current.refs++;
      current.maximum++;
      watcher.on('change', listener);
      return watcher;
    }
    const interval = selected.interval;
    if (typeof interval !== 'number') {
      throw invalid('ERR_INVALID_ARG_TYPE', 'watchFile interval must be a number');
    }
    if (!Number.isInteger(interval) || interval < 0 || interval > 4294967295) {
      throw invalid('ERR_OUT_OF_RANGE', 'watchFile interval must be a uint32', RangeError);
    }
    // Node accepts arbitrary bigint values but selects BigIntStats only for exact true.
    watcher = new StatWatcher(token, name, selected.bigint === true);
    const current = state(watcher);
    watchers.set(name, watcher);
    try {
      const result = native.statWatchStart(
        nativePath(name),
        interval,
        !!selected.persistent,
        (status, currentRaw, previousRaw) => notify(watcher, status, currentRaw, previousRaw)
      );
      if (result.status) {
        throw native.statWatchError(result.status, name);
      }
      if (current.stopped || watchers.get(name) !== watcher) {
        // A reentrant peer may admit after stop; retire the newly returned native handle too.
        native.statWatchStop(result.id);
        throw invalid('ERR_MITE_STAT_WATCH_CLOSED', 'Stat watcher stopped during admission', Error);
      }
      current.id = result.id;
      if (!selected.persistent) {
        current.refs = 0;
      }
      watcher.on('change', listener);
      if (current.stopped || watchers.get(name) !== watcher) {
        throw invalid(
          'ERR_MITE_STAT_WATCH_CLOSED',
          'Stat watcher stopped while installing listener',
          Error
        );
      }
      current.admitted = true;
      if (current.early.length) {
        schedule(watcher);
      }
      return watcher;
    } catch (failure) {
      if (watchers.get(name) === watcher) {
        watchers.delete(name);
      }
      watcher.removeAllListeners('change');
      watcher.stop();
      throw failure;
    }
  }

  function unwatchFile(value, listener) {
    const name = filename(value);
    const watcher = watchers.get(name);
    if (!watcher) {
      return;
    }
    const current = state(watcher);
    if (typeof listener === 'function') {
      const before = watcher.listenerCount('change');
      watcher.removeListener('change', listener);
      if (watcher.listenerCount('change') < before) {
        current.maximum--;
        watcher.unref();
      }
    } else {
      watcher.removeAllListeners('change');
      current.refs = 0;
      current.maximum = 0;
      if (current.id !== null) {
        native.statWatchRef(current.id, false);
      }
    }
    if (watcher.listenerCount('change') === 0) {
      watcher.stop();
      if (watchers.get(name) === watcher) {
        watchers.delete(name);
      }
    }
  }

  return { watchFile, unwatchFile };
}
