/** Main-process desktop requests. Native code owns OS work; cancellation only revokes delivery. */
export default function createShell(native, isExited, error, unsupported) {
  const pending = new Map();
  const byteLength = Buffer.byteLength.bind(Buffer);
  let stopped = false;
  let next = 0;

  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'Shell application has exited');
    }
  }

  function receiver(value) {
    if (value !== shell) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal shell receiver');
    }
    alive();
  }

  function wellFormed(value) {
    // Native UTF-8 conversion must not silently replace lone UTF-16 surrogates.
    for (let index = 0; index < value.length; index++) {
      const unit = value.charCodeAt(index);
      if (unit >= 0xd800 && unit <= 0xdbff) {
        const low = value.charCodeAt(++index);
        if (!(low >= 0xdc00 && low <= 0xdfff)) {
          return false;
        }
      } else if (unit >= 0xdc00 && unit <= 0xdfff) {
        return false;
      }
    }
    return true;
  }

  function text(value, label, maximum) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', label + ' must be a string');
    }
    if (!value) {
      throw error('ERR_INVALID_ARG_VALUE', label + ' must not be empty');
    }
    if (value.includes('\0')) {
      throw error('ERR_INVALID_ARG_VALUE', label + ' contains NUL');
    }
    if (byteLength(value, 'utf8') > maximum) {
      throw error('ERR_OUT_OF_RANGE', label + ' exceeds its UTF-8 budget');
    }
    if (!wellFormed(value)) {
      throw error('ERR_INVALID_ARG_VALUE', label + ' contains an unpaired surrogate');
    }
    return value;
  }

  function options(value) {
    if (value === undefined) {
      return true;
    }
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'Shell options must be an object');
    }
    for (const key of Reflect.ownKeys(value)) {
      if (key !== 'activate') {
        unsupported('shell.openExternal option ' + String(key));
      }
    }
    const activate = value.activate;
    if (activate !== undefined && typeof activate !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', 'Shell activate must be a boolean');
    }
    return activate === undefined ? true : activate;
  }

  function cancel(id) {
    try {
      native.shellCancel?.(id);
    } catch {
      // Revocation must settle promises even if native teardown is already in progress.
    }
  }

  function request(kind, payload) {
    alive();
    if (pending.size >= 64 || next >= 2147483647) {
      throw error('ERR_FIREFLY_SHELL_LIMIT', 'Shell request budget exhausted');
    }
    const id = ++next;
    const json = JSON.stringify(payload);
    return new Promise((resolve, reject) => {
      const entry = { kind, resolve, reject, admitted: false, early: undefined };
      pending.set(id, entry);
      try {
        const start = native.shellRequest;
        alive();
        if (typeof start !== 'function') {
          unsupported('shell.' + kind);
        }
        start.call(native, id, kind, json);
        alive();
        if (pending.get(id) !== entry) {
          return;
        }
        entry.admitted = true;
        if (entry.early !== undefined) {
          const result = entry.early;

          queueMicrotask(() => complete(id, entry, result));
        }
      } catch (failure) {
        if (pending.get(id) !== entry) {
          return;
        }
        pending.delete(id);
        cancel(id);
        if (stopped || isExited()) {
          reject(error('ERR_FIREFLY_APP_EXITED', 'Application exited during shell admission'));
        } else {
          reject(failure);
        }
      }
    });
  }

  function complete(id, entry, result) {
    if (pending.get(id) !== entry) {
      return;
    }
    let failure;
    try {
      alive();
      if (
        typeof result.ok !== 'boolean' ||
        (result.message !== undefined && typeof result.message !== 'string') ||
        (result.code !== undefined && typeof result.code !== 'string') ||
        (result.ok && (result.message !== undefined || result.code !== undefined)) ||
        (result.message !== undefined &&
          (result.message.includes('\0') ||
            byteLength(result.message, 'utf8') > 65536 ||
            !wellFormed(result.message))) ||
        (result.code !== undefined && !/^[A-Z][A-Z0-9_]{0,127}$/.test(result.code)) ||
        (!result.ok && !result.message)
      ) {
        throw error('ERR_FIREFLY_SHELL_DATA', 'Invalid native shell completion');
      }
      alive();
    } catch (caught) {
      failure = caught;
    }
    if (pending.get(id) !== entry) {
      return;
    }
    pending.delete(id);
    if (failure) {
      cancel(id);
      entry.reject(failure);
    } else if (
      !result.ok &&
      [
        'ERR_FIREFLY_SHELL_UNAVAILABLE',
        'ERR_FIREFLY_SHELL_LIMIT',
        'ERR_FIREFLY_SHELL_DATA',
        'ERR_FIREFLY_APP_EXITED',
        'ERR_FIREFLY_UNSUPPORTED_API'
      ].includes(result.code)
    ) {
      entry.reject(error(result.code, result.message));
    } else if (entry.kind === 'openPath') {
      entry.resolve(result.ok ? '' : result.message);
    } else if (result.ok) {
      entry.resolve();
    } else {
      entry.reject(error(result.code || 'ERR_FIREFLY_SHELL_FAILED', result.message));
    }
  }

  function dispatch(value) {
    const id = value?.id;
    const entry = pending.get(id);
    if (!entry) {
      return;
    }
    if (entry.early !== undefined) {
      return;
    }
    // Snapshot once: no native callback or option getter remains live across admission.
    let result;
    try {
      result = { ok: value.ok, message: value.message, code: value.code };
    } catch {
      result = {};
    }
    if (!entry.admitted) {
      if (entry.early === undefined) {
        entry.early = result;
      }
    } else {
      complete(id, entry, result);
    }
  }

  const shell = {
    showItemInFolder(path) {
      receiver(this);
      const value = text(path, 'Shell path', 32768);
      const show = native.shellShowItemInFolder;
      alive();
      if (typeof show !== 'function') {
        return unsupported('shell.showItemInFolder');
      }
      show.call(native, value);
      alive();
    },

    async openPath(path) {
      receiver(this);
      return request('openPath', { path: text(path, 'Shell path', 32768) });
    },

    async openExternal(url, value) {
      receiver(this);
      const snapshot = { url: text(url, 'Shell URL', 65536), activate: options(value) };
      alive();
      return request('openExternal', snapshot);
    },

    async trashItem(path) {
      receiver(this);
      return request('trashItem', { path: text(path, 'Shell path', 32768) });
    },

    beep() {
      receiver(this);
      return unsupported('shell.beep');
    },

    writeShortcutLink() {
      receiver(this);
      return unsupported('shell.writeShortcutLink');
    },

    readShortcutLink() {
      receiver(this);
      return unsupported('shell.readShortcutLink');
    }
  };

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const requests = [...pending];
    pending.clear();
    for (const [id, entry] of requests) {
      entry.reject(error('ERR_FIREFLY_APP_EXITED', 'Application exited during shell request'));
      cancel(id);
    }
    try {
      native.shellShutdown?.();
    } catch {
      // OS work cannot be rolled back; application teardown must continue.
    }
  }

  return { shell, dispatch, stop };
}
