/** Bounded defaults and passive native observations; this facade never grants media permissions. */
export default function createSystemPreferences(native, isExited, error, unsupported) {
  const subscriptions = new Map();
  const byteLength = Buffer.byteLength.bind(Buffer);
  let next = 0;
  let stopped = false;

  function data(message) {
    return error('ERR_FIREFLY_SYSTEM_PREFERENCES_DATA', message);
  }

  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'System preferences application has exited');
    }
  }

  function receiver(value) {
    if (value !== systemPreferences) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal systemPreferences receiver');
    }
    alive();
  }

  function text(value, maximum, empty = false, allowNul = false) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'System preference text must be a string');
    }
    if ((!empty && !value) || (!allowNul && value.includes('\0'))) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid system preference text');
    }
    if (byteLength(value, 'utf8') > maximum) {
      throw error('ERR_OUT_OF_RANGE', 'System preference text exceeds its UTF-8 budget');
    }
    for (let index = 0; index < value.length; index++) {
      const unit = value.charCodeAt(index);
      if (unit >= 0xd800 && unit <= 0xdbff) {
        const low = value.charCodeAt(++index);
        if (!(low >= 0xdc00 && low <= 0xdfff)) {
          throw error('ERR_INVALID_ARG_VALUE', 'Unpaired system preference surrogate');
        }
      } else if (unit >= 0xdc00 && unit <= 0xdfff) {
        throw error('ERR_INVALID_ARG_VALUE', 'Unpaired system preference surrogate');
      }
    }
    return value;
  }

  function type(value) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'System preference type must be a string');
    }
    if (value !== 'string' && value !== 'boolean') {
      unsupported('systemPreferences default type ' + value);
    }
    return value;
  }

  function call(name, ...args) {
    const method = native[name];
    alive();
    if (typeof method !== 'function') {
      return unsupported('systemPreferences native ' + name);
    }
    const result = method.call(native, ...args);
    alive();
    return result;
  }

  function snapshot(raw) {
    try {
      if (typeof raw !== 'string' || byteLength(raw, 'utf8') > 524288) {
        throw data('Invalid preference snapshot size');
      }
      const result = JSON.parse(raw);
      if (
        !result ||
        typeof result !== 'object' ||
        Array.isArray(result) ||
        !Object.hasOwn(result, 'value')
      ) {
        throw data('Invalid preference snapshot shape');
      }
      return result.value;
    } catch {
      throw data('Invalid native preference snapshot');
    }
  }

  function release(id, entry) {
    if (subscriptions.get(id) !== entry) {
      return;
    }
    subscriptions.delete(id);
    entry.callback = null;
    entry.early.length = 0;
    try {
      native.preferencesUnsubscribe?.(id, entry.kind);
    } catch {
      // Local callbacks are revoked even if native observer removal fails.
    }
  }

  function subscribe(kind, name, callback) {
    if (name !== null) {
      text(name, 4096);
    }
    if (typeof callback !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'System preference callback must be a function');
    }
    alive();
    if (subscriptions.size >= 64 || next > 2147483647) {
      throw error(
        'ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT',
        'Preference subscription budget exhausted'
      );
    }
    const id = next++;
    const entry = { kind, name, callback, admitted: false, early: [] };
    subscriptions.set(id, entry);
    try {
      call('preferencesSubscribe', id, kind, name);
      if (subscriptions.get(id) !== entry) {
        throw data('Native preference subscription retired during admission');
      }
      entry.admitted = true;
      if (entry.early.length) {
        schedule(id, entry);
      }
      return id;
    } catch (failure) {
      if (subscriptions.get(id) === entry) {
        release(id, entry);
      } else if (stopped || isExited()) {
        try {
          // A broken reentrant peer could admit this observer after its first shutdown.
          native.preferencesUnsubscribe?.(id, kind);
        } catch {
          // The callback remains revoked even if this final removal also fails.
        }
      }
      throw failure;
    }
  }

  function schedule(id, entry) {
    // Keep the subscription call's callback parameter out of the queued closure's scope.
    queueMicrotask(() => {
      while (subscriptions.get(id) === entry && entry.early.length) {
        deliver(id, entry, entry.early.shift());
      }
    });
  }

  function unsubscribe(kind, id) {
    if (typeof id !== 'number' || !Number.isInteger(id) || Object.is(id, -0)) {
      throw error('ERR_INVALID_ARG_TYPE', 'Preference subscription ID must be an int32');
    }
    if (id < -2147483648 || id > 2147483647) {
      throw error('ERR_OUT_OF_RANGE', 'Preference subscription ID exceeds int32 range');
    }
    const entry = subscriptions.get(id);
    if (entry?.kind === kind) {
      // Revoke before native removal; callbacks cannot resurrect a removed subscription.
      subscriptions.delete(id);
      entry.callback = null;
      entry.early.length = 0;
      call('preferencesUnsubscribe', id, kind);
    }
  }

  function userInfo(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw data('Notification userInfo must be a dictionary');
    }
    let nodes = 0;
    const seen = new Set();

    function clone(item, depth) {
      if (++nodes > 1024 || depth > 8) {
        throw data('Notification JSON graph exceeds its budget');
      }
      if (
        item === null ||
        typeof item === 'boolean' ||
        (typeof item === 'number' && Number.isFinite(item))
      ) {
        return item;
      }
      if (typeof item === 'string') {
        return text(item, 65536, true, true);
      }
      if (!item || typeof item !== 'object' || seen.has(item)) {
        throw data('Notification userInfo is not JSON data');
      }
      const array = Array.isArray(item);
      const prototype = Object.getPrototypeOf(item);
      if (!array && prototype !== Object.prototype && prototype !== null) {
        throw data('Notification userInfo has an unsupported prototype');
      }
      seen.add(item);
      const output = array ? [] : {};
      const keys = Reflect.ownKeys(item);
      for (const key of keys) {
        if (array && key === 'length') {
          continue;
        }
        if (typeof key !== 'string' || (array && !/^(0|[1-9][0-9]*)$/.test(key))) {
          throw data('Notification userInfo has an invalid key');
        }
        text(key, 65536, true, true);
        const descriptor = Object.getOwnPropertyDescriptor(item, key);
        if (!descriptor || !Object.hasOwn(descriptor, 'value') || !descriptor.enumerable) {
          throw data('Notification userInfo must contain data properties');
        }
        Object.defineProperty(output, key, {
          value: clone(descriptor.value, depth + 1),
          enumerable: true,
          writable: true,
          configurable: true
        });
      }
      if (array && output.length !== keys.length - 1) {
        throw data('Notification userInfo arrays must be dense');
      }
      seen.delete(item);
      return output;
    }

    const result = clone(value, 0);
    if (byteLength(JSON.stringify(result), 'utf8') > 65536) {
      throw data('Notification userInfo exceeds its JSON byte budget');
    }
    return result;
  }

  function packet(value, entry) {
    try {
      const code = value.code;
      const event = value.event;
      const info = value.userInfo;
      const object = value.object;
      if (code !== undefined) {
        throw data('Native preference notification delivery failed');
      }
      text(event, 4096);
      text(object, 4096, true);
      if (entry.name !== null && event !== entry.name) {
        throw data('Native preference notification name mismatch');
      }
      return { event, userInfo: userInfo(info), object };
    } catch {
      throw data('Invalid native preference notification');
    }
  }

  function deliver(id, entry, value) {
    alive();
    if (subscriptions.get(id) !== entry) {
      return;
    }
    entry.callback(value.event, value.userInfo, value.object);
  }

  function dispatch(value) {
    const id = value?.id;
    const entry = subscriptions.get(id);
    if (!entry) {
      return;
    }
    let result;
    try {
      result = packet(value, entry);
      alive();
      if (subscriptions.get(id) !== entry) {
        return;
      }
      if (!entry.admitted || entry.early.length) {
        if (entry.early.length >= 64) {
          throw data('Too many early preference notifications');
        }
        entry.early.push(result);
        return;
      }
    } catch (failure) {
      release(id, entry);
      throw failure;
    }
    deliver(id, entry, result);
  }

  const systemPreferences = {
    getUserDefault(name, kind) {
      receiver(this);
      text(name, 4096);
      type(kind);
      const value = snapshot(call('preferencesGetDefault', name, kind));
      try {
        if (kind === 'string') {
          text(value, 65536, true);
        } else if (typeof value !== 'boolean') {
          throw data('Native boolean default has invalid type');
        }
      } catch {
        throw data('Invalid native default value');
      }
      return value;
    },

    setUserDefault(name, kind, value) {
      receiver(this);
      text(name, 4096);
      type(kind);
      if (kind === 'string') {
        text(value, 65536, true);
      } else if (typeof value !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'Boolean default requires a boolean');
      }
      call('preferencesSetDefault', name, kind, JSON.stringify(value));
    },

    getMediaAccessStatus(kind) {
      receiver(this);
      if (typeof kind !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Media type must be a string');
      }
      if (!['camera', 'microphone', 'screen'].includes(kind)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid media type');
      }
      const value = snapshot(call('preferencesMediaStatus', kind));
      if (!['not-determined', 'granted', 'denied', 'restricted', 'unknown'].includes(value)) {
        throw data('Invalid native media status');
      }
      return value;
    }
  };

  for (const [suffix, kind] of [
    ['', 'distributed'],
    ['Local', 'local'],
    ['Workspace', 'workspace']
  ]) {
    systemPreferences['subscribe' + suffix + 'Notification'] = function (name, callback) {
      receiver(this);
      return subscribe(kind, name, callback);
    };

    systemPreferences['unsubscribe' + suffix + 'Notification'] = function (id) {
      receiver(this);
      unsubscribe(kind, id);
    };
  }

  for (const name of [
    'postNotification',
    'postLocalNotification',
    'postWorkspaceNotification',
    'registerDefaults',
    'removeUserDefault',
    'askForMediaAccess',
    'isSwipeTrackingFromScrollEventsEnabled',
    'getAccentColor',
    'getColor',
    'getSystemColor',
    'getEffectiveAppearance',
    'canPromptTouchID',
    'promptTouchID',
    'isTrustedAccessibilityClient',
    'getAnimationSettings',
    'on',
    'once',
    'addListener',
    'removeListener',
    'off',
    'emit',
    'removeAllListeners',
    'prependListener',
    'prependOnceListener'
  ]) {
    systemPreferences[name] = function () {
      receiver(this);
      return unsupported('systemPreferences.' + name);
    };
  }

  for (const name of ['effectiveAppearance', 'accessibilityDisplayShouldReduceTransparency']) {
    Object.defineProperty(systemPreferences, name, {
      get() {
        receiver(this);
        return unsupported('systemPreferences.' + name);
      }
    });
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const entries = [...subscriptions];
    subscriptions.clear();
    for (const [, entry] of entries) {
      entry.callback = null;
      entry.early.length = 0;
    }
    for (const [id, entry] of entries) {
      try {
        native.preferencesUnsubscribe?.(id, entry.kind);
      } catch {
        // All callback references were already revoked before crossing this boundary.
      }
    }
    try {
      native.preferencesShutdown?.();
    } catch {
      // Host teardown continues even if a native observer cannot be removed.
    }
  }

  return { systemPreferences, dispatch, stop };
}
