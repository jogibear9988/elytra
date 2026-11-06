/**
 * Main-process invoke handlers and bounded pending replies. Native routing supplies
 * sender metadata; application handlers decide authorization. Lifecycle teardown
 * cancels pending replies without trying to undo already-started handler effects.
 */
export default function createMainIpc(native, EventEmitter, error) {
  const ipcHandlers = new Map();
  const ipcRequests = new Map();

  function channelName(channel) {
    if (typeof channel !== 'string' || channel.length > 256) {
      throw error('ERR_INVALID_ARG_TYPE', 'channel must be a string of at most 256 UTF-16 units');
    }
    return channel;
  }

  class IpcMain extends EventEmitter {
    handle(channel, listener) {
      this._register(channel, listener, false);
    }

    handleOnce(channel, listener) {
      this._register(channel, listener, true);
    }

    _register(channel, listener, once) {
      channelName(channel);
      if (typeof listener !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'IPC handler must be a function');
      }
      if (ipcHandlers.has(channel)) {
        throw new Error('A handler is already registered for this channel');
      }
      ipcHandlers.set(channel, { listener, once });
    }

    removeHandler(channel) {
      ipcHandlers.delete(channelName(channel));
    }
  }
  const ipcMain = new IpcMain();

  /** Reclaim timers and sender snapshots before a new document can receive replies. */
  function cancelIpc(window) {
    for (const [key, pending] of ipcRequests) {
      if (pending.window === window) {
        clearTimeout(pending.timer);
        ipcRequests.delete(key);
      }
    }
    window.webContents._ipcFrame = null;
  }

  /** Native code supplies document identity and URL; payload arguments never identify senders. */
  function dispatchIpc(window, data) {
    const key = `${window.id}/${data.document}/${data.request}`;

    const finish = (success, value) => {
      const pending = ipcRequests.get(key);
      if (!pending) {
        return;
      }
      ipcRequests.delete(key);
      clearTimeout(pending.timer);
      let wire;
      try {
        wire = native.encodeIpc(value);
      } catch {
        success = false;
        wire = native.encodeIpc('IPC result is not supported bounded data');
      }
      native.replyIpc(window.id, data.document, data.realm, data.request, success, wire);
    };

    if (ipcRequests.size >= 64) {
      native.replyIpc(
        window.id,
        data.document,
        data.realm,
        data.request,
        false,
        native.encodeIpc('IPC pending request budget exceeded')
      );
      return;
    }
    const timer = setTimeout(() => finish(false, 'IPC handler timed out'), 30000);
    ipcRequests.set(key, { window, timer });
    try {
      const args = native.decodeIpc(data.wire);
      if (!Array.isArray(args) || args.length < 1 || args.length > 65) {
        throw Error('Invalid IPC arguments');
      }
      const channel = channelName(args.shift());
      const record = ipcHandlers.get(channel);
      if (!record) {
        finish(false, `No handler registered for '${channel}'`);
        return;
      }
      let frame = window.webContents._ipcFrame;
      if (!frame || frame._document !== data.document || frame._realm !== data.realm) {
        frame = {
          _document: data.document,
          _realm: data.realm,
          url: data.url,

          isDestroyed() {
            return window.isDestroyed() || window.webContents._ipcFrame !== frame;
          }
        };
        window.webContents._ipcFrame = frame;
      }
      // Same-document URL changes retain the endpoint, but must not retain an old URL.
      frame.url = data.url;
      if (record.once) {
        ipcHandlers.delete(channel);
      }
      const event = { sender: window.webContents, senderFrame: frame };
      const result = Reflect.apply(record.listener, undefined, [event, ...args]);

      Promise.resolve(result).then(
        (value) => finish(true, value),
        (failure) => {
          let message = 'IPC handler rejected';
          try {
            if (typeof failure?.message === 'string') {
              message = failure.message.slice(0, 1024);
            }
          } catch {}
          finish(false, message);
        }
      );
    } catch (failure) {
      let message = 'IPC handler rejected';
      try {
        if (typeof failure?.message === 'string') {
          message = failure.message.slice(0, 1024);
        }
      } catch {}
      finish(false, message);
    }
  }

  return { ipcMain, cancel: cancelIpc, dispatch: dispatchIpc };
}
