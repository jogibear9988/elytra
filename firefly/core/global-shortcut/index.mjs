/** Own successful OS hotkey admissions and callbacks; no application-local shortcut emulation. */
export default function createGlobalShortcuts(
  native,
  isReady,
  isExited,
  error,
  unsupported,
  platform
) {
  const tokens = new Map();
  const chords = new Map();
  // Retain only native ownership tokens after callback revocation.
  const retired = new Set();
  const releasing = new Set();
  const modifiers = new Map([
    ['shift', 1],
    ['control', 2],
    ['ctrl', 2],
    ['alt', 4],
    ['option', 4],
    ['command', 8],
    ['cmd', 8],
    ['meta', 8],
    ['super', 8],
    ['commandorcontrol', platform === 'darwin' ? 8 : 2],
    ['cmdorctrl', platform === 'darwin' ? 8 : 2]
  ]);
  const names = new Map([
    ['space', 'Space'],
    ['tab', 'Tab'],
    ['enter', 'Enter'],
    ['return', 'Enter'],
    ['escape', 'Escape'],
    ['esc', 'Escape'],
    ['backspace', 'Backspace'],
    ['delete', 'Delete'],
    ['insert', 'Insert'],
    ['home', 'Home'],
    ['end', 'End'],
    ['pageup', 'PageUp'],
    ['pagedown', 'PageDown'],
    ['up', 'Up'],
    ['down', 'Down'],
    ['left', 'Left'],
    ['right', 'Right'],
    ['plus', '+']
  ]);
  let next = 0;
  let stopped = false;

  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'Global shortcuts application has exited');
    }
    if (ready && !isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'globalShortcut requires app readiness');
    }
  }

  function receiver(value, ready = true) {
    if (value !== globalShortcut) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal globalShortcut receiver');
    }
    alive(ready);
  }

  function parse(value) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'Accelerator must be a string');
    }
    if (value.length > 4096) {
      throw error('ERR_OUT_OF_RANGE', 'Accelerator exceeds its text budget');
    }
    if (!value || /[^\x20-\x7e]/.test(value)) {
      throw error('ERR_INVALID_ARG_VALUE', 'Accelerator must contain ASCII key names');
    }
    let key;
    let flags = 0;
    for (const part of value
      .split('+')
      .map((piece) => piece.trim())
      .filter(Boolean)) {
      const lower = part.toLowerCase();
      if (modifiers.has(lower)) {
        flags |= modifiers.get(lower);
        continue;
      }
      if (
        lower === 'altgr' ||
        /^(media|volume|num)/.test(lower) ||
        ['capslock', 'scrolllock', 'printscreen'].includes(lower) ||
        /^f(?:2[1-4])$/.test(lower)
      ) {
        unsupported('globalShortcut key ' + part);
      }
      let current = names.get(lower);
      if (!current && /^[a-z0-9]$/i.test(part)) {
        current = part.toUpperCase();
      }
      if (!current && /^f(?:[1-9]|1[0-9]|20)$/.test(lower)) {
        current = lower.toUpperCase();
      }
      if (!current && part.length === 1 && /^[!"#$%&'()*,\-./:;<=>?@[\]\\^_`{|}~]$/.test(part)) {
        current = part;
      }
      if (!current || key !== undefined) {
        throw error('ERR_INVALID_ARG_VALUE', 'Accelerator must contain exactly one supported key');
      }
      key = current;
    }
    if (!key) {
      throw error('ERR_INVALID_ARG_VALUE', 'Accelerator does not contain a key');
    }
    return { key, modifiers: flags, canonical: flags + ':' + key };
  }

  function release(token) {
    if (releasing.has(token)) {
      return;
    }
    if (!stopped) {
      retired.add(token);
    }
    releasing.add(token);
    try {
      native.globalShortcutUnregister(token);
      retired.delete(token);
    } finally {
      releasing.delete(token);
    }
  }

  function retryRetired() {
    if (releasing.size) {
      return;
    }
    let failure;
    // One bounded snapshot pass; failed releases remain owned without a callback.
    for (const token of [...retired]) {
      if (stopped || isExited()) {
        break;
      }
      try {
        release(token);
      } catch (caught) {
        failure ??= caught;
      }
    }
    alive();
    if (failure) {
      throw failure;
    }
  }

  function remove(entry, notify = true) {
    if (tokens.get(entry.token) !== entry) {
      return;
    }
    tokens.delete(entry.token);
    if (chords.get(entry.chord.canonical) === entry) {
      chords.delete(entry.chord.canonical);
    }
    entry.callback = null;
    entry.early = 0;
    if (notify) {
      release(entry.token);
    }
  }

  function discard(entry) {
    try {
      remove(entry);
    } catch {
      /* Rollback must continue for all owned admissions. */
    }
  }

  function admit(chord, callback) {
    alive();
    retryRetired();
    // A native release may reenter JS. Never admit while its ownership is unresolved.
    if (releasing.size) {
      throw error('ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT', 'Native shortcut release is in progress');
    }
    if (chords.has(chord.canonical)) {
      return null;
    }
    if (tokens.size + retired.size >= 64 || next >= 2147483647) {
      throw error('ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT', 'Global shortcut budget exhausted');
    }
    const register = native.globalShortcutRegister;
    if (
      typeof register !== 'function' ||
      typeof native.globalShortcutUnregister !== 'function' ||
      typeof native.globalShortcutShutdown !== 'function'
    ) {
      unsupported('globalShortcut native ownership');
    }
    alive();
    const entry = { token: ++next, chord, callback, admitted: false, early: 0 };
    tokens.set(entry.token, entry);
    chords.set(chord.canonical, entry);
    try {
      const result = register.call(native, entry.token, chord.key, chord.modifiers);
      if (stopped || isExited() || tokens.get(entry.token) !== entry) {
        try {
          release(entry.token);
        } catch {
          /* Revoke late admission best effort. */
        }
        alive();
        throw error(
          'ERR_FIREFLY_GLOBAL_SHORTCUT_DATA',
          'Registration was retired during native admission'
        );
      }
      alive();
      if (typeof result !== 'boolean') {
        throw error('ERR_FIREFLY_GLOBAL_SHORTCUT_DATA', 'Invalid native shortcut admission result');
      }
      if (!result) {
        remove(entry, false);
        return null;
      }
      entry.admitted = true;
      if (entry.early) {
        schedule(entry);
      }
      return entry;
    } catch (failure) {
      discard(entry);
      throw failure;
    }
  }

  function schedule(entry) {
    queueMicrotask(() => {
      while (tokens.get(entry.token) === entry && entry.early > 0) {
        entry.early--;
        invoke(entry);
      }
    });
  }

  function invoke(entry) {
    if (stopped || isExited() || tokens.get(entry.token) !== entry || !entry.admitted) {
      return;
    }
    const callback = entry.callback;
    callback();
  }

  function dispatch(value) {
    const entry = tokens.get(value?.token);
    if (!entry || stopped || isExited()) {
      return;
    }
    if (!entry.admitted) {
      if (entry.early >= 64) {
        discard(entry);
        throw error(
          'ERR_FIREFLY_GLOBAL_SHORTCUT_DATA',
          'Native shortcut admission event budget exhausted'
        );
      }
      entry.early++;
    } else {
      invoke(entry);
    }
  }

  const globalShortcut = {
    register(accelerator, callback) {
      receiver(this);
      const chord = parse(accelerator);
      if (typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'Shortcut callback must be a function');
      }
      return admit(chord, callback) !== null;
    },

    registerAll(accelerators, callback) {
      receiver(this);
      if (!Array.isArray(accelerators)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Accelerators must be an array');
      }
      if (accelerators.length > 64) {
        throw error('ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT', 'Too many accelerators');
      }
      if (typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'Shortcut callback must be a function');
      }
      const selected = Array.from(accelerators, parse);
      alive();
      const admitted = [];
      try {
        for (const chord of selected) {
          const entry = admit(chord, callback);
          if (!entry) {
            for (const owned of admitted) {
              discard(owned);
            }
            return false;
          }
          admitted.push(entry);
        }
        return true;
      } catch (failure) {
        for (const entry of admitted) {
          discard(entry);
        }
        throw failure;
      }
    },

    isRegistered(accelerator) {
      receiver(this, false);
      return chords.get(parse(accelerator).canonical)?.admitted === true;
    },

    unregister(accelerator) {
      receiver(this);
      const entry = chords.get(parse(accelerator).canonical);
      if (entry) {
        remove(entry);
      }
      alive();
    },

    unregisterAll() {
      receiver(this);
      let failure;
      try {
        retryRetired();
      } catch (caught) {
        failure = caught;
      }
      for (const entry of [...tokens.values()]) {
        try {
          remove(entry);
        } catch (caught) {
          failure ??= caught;
        }
      }
      alive();
      if (failure) {
        throw failure;
      }
    },

    setSuspended() {
      receiver(this);
      return unsupported('globalShortcut.setSuspended');
    },

    isSuspended() {
      receiver(this);
      return unsupported('globalShortcut.isSuspended');
    }
  };

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const entry of tokens.values()) {
      entry.callback = null;
      entry.early = 0;
    }
    tokens.clear();
    chords.clear();
    retired.clear();
    try {
      native.globalShortcutShutdown?.();
    } catch {
      /* Application cleanup must continue. */
    }
  }

  return { globalShortcut, dispatch, stop };
}
