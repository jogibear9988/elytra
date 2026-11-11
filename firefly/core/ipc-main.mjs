/**
 * Main-process invoke handlers and bounded pending replies. Native routing supplies
 * sender metadata; application handlers decide authorization. Lifecycle teardown
 * cancels pending replies without trying to undo already-started handler effects.
 */
export default function createMainIpc(native, EventEmitter, error, frameForDocument) {
  const ipcHandlers = new Map();
  const ipcRequests = new Map();
  const diagnostic = process.env.FIREFLY_IPC_BYTES_DIAGNOSTIC === '1';

  // Deliberately exclude channels, values, error messages and arbitrary user fields.
  function failed(stage, failure) {
    if (!diagnostic) {
      return;
    }
    try {
      const descriptor =
        failure && typeof failure === 'object'
          ? Object.getOwnPropertyDescriptor(failure, 'code')
          : undefined;
      const code =
        descriptor &&
        'value' in descriptor &&
        [
          'ERR_FIREFLY_IPC_SERIALIZATION',
          'ERR_FIREFLY_OBJECT_DESTROYED',
          'ERR_FIREFLY_UNSUPPORTED_API'
        ].includes(descriptor.value)
          ? descriptor.value
          : 'UNCLASSIFIED';
      console.warn(`FIREFLY_IPC_FAILURE stage=${stage} code=${code}`);
    } catch {}
  }

  function channelName(channel) {
    if (typeof channel !== 'string' || channel.length > 256) {
      throw error('ERR_INVALID_ARG_TYPE', 'channel must be a string of at most 256 UTF-16 units');
    }
    return channel;
  }

  class IpcMain extends EventEmitter {
    constructor() {
      super();
      // Electron treats an unhandled IPC channel named error as an ordinary event.
      this.on('error', () => {});
    }

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
  }

  /** Shared native-bound sender identity for invoke handlers and event listeners. */
  function senderEvent(window, data) {
    if (typeof frameForDocument !== 'function') {
      throw error('ERR_FIREFLY_UNSUPPORTED_API', 'Canonical IPC frame provider is unavailable');
    }
    let frame;
    try {
      frame = frameForDocument(window.id, data.document);
    } catch (failure) {
      if (failure?.code === 'ERR_FIREFLY_OBJECT_DESTROYED') {
        return undefined;
      }
      throw failure;
    }
    if (!frame) {
      return undefined;
    } // A queued packet cannot resurrect a replaced document.
    return { sender: window.webContents, senderFrame: frame };
  }

  function dispatchEvent(window, data) {
    let args;
    let channel;
    try {
      args = native.decodeIpc(data.wire);
      if (!Array.isArray(args) || args.length < 1 || args.length > 65) {
        throw Error('Invalid IPC event arguments');
      }
      channel = channelName(args.shift());
    } catch {
      // A valid graph can still be an invalid argument envelope. Reject it
      // before EventEmitter; application listener failures remain outside this guard.
      console.warn('ERR_FIREFLY_IPC_SERIALIZATION: invalid event arguments');
      return;
    }
    // EventEmitter supplies registration order, once-before-call removal and
    // listener exception behavior. Return values never become invoke replies.
    const event = senderEvent(window, data);
    if (event) {
      ipcMain.emit(channel, event, ...args);
    }
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
      } catch (failure) {
        failed('result-serialization', failure);
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
        failed('missing-handler');
        finish(false, `No handler registered for '${channel}'`);
        return;
      }
      if (record.once) {
        ipcHandlers.delete(channel);
      }
      const event = senderEvent(window, data);
      if (!event) {
        failed('inactive-frame');
        finish(false, 'IPC sender frame is no longer active');
        return;
      }
      const result = Reflect.apply(record.listener, undefined, [event, ...args]);

      Promise.resolve(result).then(
        (value) => finish(true, value),
        (failure) => {
          failed('handler-rejection', failure);
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
      failed('dispatch', failure);
      let message = 'IPC handler rejected';
      try {
        if (typeof failure?.message === 'string') {
          message = failure.message.slice(0, 1024);
        }
      } catch {}
      finish(false, message);
    }
  }

  return { ipcMain, cancel: cancelIpc, dispatch: dispatchIpc, dispatchEvent };
}
