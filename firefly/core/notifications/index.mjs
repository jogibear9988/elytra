/** Owned notification generations: native scheduling is distinct from visible OS presentation. */
export default function createNotifications(
  native,
  EventEmitter,
  isReady,
  isExited,
  error,
  unsupported
) {
  const records = new WeakMap();
  const active = new Map();
  const bytes = globalThis.Buffer.byteLength.bind(globalThis.Buffer);
  const stringify = JSON.stringify;
  let next = 0;
  let stopped = false;

  function running() {
    if (stopped || isExited()) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Notifications cannot run after app exit');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Notification requires app readiness');
    }
  }

  function invalid(message, code = 'ERR_INVALID_ARG_TYPE') {
    return Object.assign(new TypeError(message), { code });
  }

  function text(value, name, limit) {
    if (typeof value !== 'string' || value.includes('\0')) {
      throw invalid(`${name} must be a string without NUL`);
    }
    if (bytes(value, 'utf8') > limit) {
      throw invalid(`${name} exceeds its UTF-8 byte limit`, 'ERR_OUT_OF_RANGE');
    }
    return value;
  }

  function actions(value) {
    if (!Array.isArray(value)) {
      throw invalid('actions must be an array');
    }
    const length = value.length;
    if (length > 4) {
      throw invalid('At most four notification actions are supported', 'ERR_OUT_OF_RANGE');
    }
    const result = [];
    for (let index = 0; index < length; ++index) {
      const action = value[index];
      if (!action || typeof action !== 'object') {
        throw invalid('Each action must be an object');
      }
      const type = action.type;
      const label = action.text;
      if (type !== 'button') {
        unsupported('Notification non-button actions');
      }
      for (const key of Object.keys(action)) {
        if (!['type', 'text'].includes(key)) {
          unsupported(`Notification action option ${key}`);
        }
      }
      result.push({ type, text: text(label, 'action.text', 1024) });
    }
    return result;
  }

  function properties(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw invalid('Notification options must be an object');
    }
    for (const key of Object.keys(value)) {
      if (!['title', 'body', 'silent', 'actions'].includes(key)) {
        unsupported(`Notification option ${key}`);
      }
    }
    const title = value.title;
    const body = value.body;
    const silent = value.silent;
    const buttons = value.actions;
    if (silent !== undefined && typeof silent !== 'boolean') {
      throw invalid('silent must be a boolean');
    }
    return {
      title: text(title === undefined ? '' : title, 'title', 4096),
      body: text(body === undefined ? '' : body, 'body', 65536),
      silent: silent ?? false,
      actions: buttons === undefined ? [] : actions(buttons)
    };
  }

  function record(object) {
    const value = records.get(object);
    if (!value) {
      throw invalid('Illegal Notification receiver');
    }
    return value;
  }

  // Retire authority before OS close or user listeners can reenter show/close.
  function retire(token, notify) {
    const entry = active.get(token);
    if (!entry) {
      return;
    }
    active.delete(token);
    if (entry.record.token === token) {
      entry.record.token = null;
    }
    const version = entry.record.version;
    try {
      native.notificationClose(token);
    } finally {
      if (notify && !stopped && !isExited() && entry.record.version === version) {
        entry.object.emit('close', {});
      }
    }
  }

  class Notification extends EventEmitter {
    constructor(options = {}) {
      super();
      running();
      const value = properties(options);
      // User getters in options can synchronously shut down the application.
      running();
      records.set(this, { value, token: null, version: 0 });
    }

    static isSupported() {
      if (
        stopped ||
        isExited() ||
        !isReady() ||
        typeof native.notificationSupported !== 'function'
      ) {
        return false;
      }
      const supported = native.notificationSupported() === true;
      return supported && !stopped && !isExited() && isReady();
    }

    get title() {
      return record(this).value.title;
    }

    set title(value) {
      record(this).value.title = text(value, 'title', 4096);
    }

    get body() {
      return record(this).value.body;
    }

    set body(value) {
      record(this).value.body = text(value, 'body', 65536);
    }

    get silent() {
      return record(this).value.silent;
    }

    set silent(value) {
      if (typeof value !== 'boolean') {
        throw invalid('silent must be a boolean');
      }
      record(this).value.silent = value;
    }

    get actions() {
      return record(this).value.actions.map((action) => ({ ...action }));
    }

    set actions(value) {
      record(this).value.actions = actions(value);
    }

    show() {
      running();
      const state = record(this);
      const version = ++state.version;
      if (state.token !== null) {
        retire(state.token, true);
      }
      // A close listener may have shown/reclosed this instance or exited the app.
      running();
      if (state.version !== version) {
        return;
      }
      if (active.size >= 64 || next >= 2147483647) {
        throw error('ERR_FIREFLY_NOTIFICATION_LIMIT', 'Notification quota or IDs exhausted');
      }
      const token = ++next;
      const value = {
        ...state.value,
        actions: state.value.actions.map((action) => ({ ...action }))
      };
      state.token = token;
      active.set(token, {
        object: this,
        record: state,
        shown: false,
        actionCount: value.actions.length
      });
      try {
        native.notificationShow(token, stringify(value));
      } catch (failure) {
        // Native submission failures use the asynchronous failed event. Closing
        // before this microtask revokes it just like a late OS completion.
        queueMicrotask(() =>
          dispatch({ token, type: 'failed', message: failure?.message ?? String(failure) })
        );
      }
    }

    close() {
      const state = record(this);
      ++state.version;
      if (state.token !== null) {
        retire(state.token, true);
      }
    }
  }

  // Known Electron properties outside this native slice fail rather than being
  // silently stored as inert JavaScript fields.
  for (const name of [
    'icon',
    'subtitle',
    'hasReply',
    'replyPlaceholder',
    'sound',
    'urgency',
    'timeoutType',
    'closeButtonText',
    'toastXml'
  ]) {
    Object.defineProperty(Notification.prototype, name, {
      get() {
        return unsupported(`Notification.${name}`);
      },

      set() {
        unsupported(`Notification.${name}`);
      }
    });
  }

  function dispatch(value) {
    const entry = active.get(value?.token);
    if (!entry || stopped || isExited()) {
      return;
    }
    const type = value.type;
    if (type === 'show') {
      if (!entry.shown) {
        entry.shown = true;
        entry.object.emit('show', {});
      }
      return;
    }
    let event = type;
    let message = value.message;
    const index = value.index;
    if (
      !['click', 'action', 'close', 'failed'].includes(type) ||
      (type === 'action' &&
        (!Number.isInteger(index) || index < 0 || index >= entry.actionCount)) ||
      (type === 'failed' && (typeof message !== 'string' || bytes(message, 'utf8') > 4096))
    ) {
      event = 'failed';
      message = 'Invalid native notification event';
    }
    const version = entry.record.version;
    retire(value.token, false);
    if (stopped || isExited() || entry.record.version !== version) {
      return;
    }
    if (event === 'action') {
      entry.object.emit('action', { actionIndex: index }, index);
    } else if (event === 'failed') {
      entry.object.emit('failed', {}, message);
    } else {
      entry.object.emit(event, {});
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const [token, entry] of [...active]) {
      ++entry.record.version;
      try {
        retire(token, false);
      } catch {
        // Teardown still revokes every token if a native close has failed.
      }
    }
  }

  return { Notification, dispatch, stop };
}
