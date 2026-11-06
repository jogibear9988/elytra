/**
 * Bridge portable Node streams and the realm's captured Web Stream constructors.
 * Ownership follows the adapter: destroy cancels/aborts Web endpoints; Web cancel
 * destroys Node resources. Never collect a whole body merely to cross the boundary.
 */
export default function installWebAdapters(stream, host) {
  const { ReadableStream, WritableStream } = host.webStreams;
  const { Readable, Writable, Duplex, finished } = stream;
  const tick = host.process.nextTick;
  const abort = host.abort;
  const Buffer = host.require('node:buffer').Buffer;
  const readableLocked = Object.getOwnPropertyDescriptor(ReadableStream.prototype, 'locked').get;
  const writableLocked = Object.getOwnPropertyDescriptor(WritableStream.prototype, 'locked').get;

  function failure(Type, code, message) {
    return Object.assign(new Type(message), { code });
  }

  function abortFailure(cause) {
    return Object.assign(failure(Error, 'ABORT_ERR', 'The operation was aborted'), {
      name: 'AbortError',
      cause
    });
  }

  function webEndpoint(value, getter, name) {
    let locked;
    try {
      locked = getter.call(value);
    } catch {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', `${name} must be a Web Stream`);
    }
    if (locked) {
      throw failure(TypeError, 'ERR_INVALID_STATE', `${name} is locked`);
    }
  }

  function options(value) {
    if (value === undefined) {
      return {};
    }
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', 'options must be an object');
    }
    if (value.signal !== undefined && !abort.isSignal(value.signal)) {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', 'signal must be an AbortSignal');
    }
    return value;
  }

  /** One state machine also serves Duplex: each endpoint has one exclusive lock. */
  function fromWeb(readable, writable, value, Type) {
    const opts = options(value);
    if (readable) {
      webEndpoint(readable, readableLocked, 'readableStream');
    }
    if (writable) {
      webEndpoint(writable, writableLocked, 'writableStream');
    }
    let reader;
    let writer;
    let reading = false;
    let readDone = !readable;
    let writeDone = !writable;

    let unsubscribe = () => {};

    const node = new Type({
      objectMode: opts.objectMode,
      highWaterMark: opts.highWaterMark,
      encoding: opts.encoding,
      decodeStrings: opts.decodeStrings,
      ...(Type === Duplex ? { allowHalfOpen: opts.allowHalfOpen ?? false } : {}),

      read() {
        if (reading || readDone || this.destroyed) {
          return;
        }
        reading = true;

        reader
          .read()
          .then(({ value, done }) => {
            reading = false;
            if (this.destroyed) {
              return;
            }
            if (done) {
              readDone = true;
            }
            this.push(done ? null : value);
          })
          .catch((error) => {
            reading = false;
            this.destroy(error);
          });
      },

      write(chunk, encoding, callback) {
        writer.write(chunk).then(() => callback(), callback);
      },

      final(callback) {
        writer.close().then(() => {
          writeDone = true;
          callback();
        }, callback);
      },

      destroy(error, callback) {
        unsubscribe();
        // Do not release locks: Node's fromWeb adapters retain endpoint ownership
        // even after completion. Observe both shutdowns before emitting close.
        const pending = [];
        if (reader && !readDone) {
          pending.push(reader.cancel(error));
        }
        if (writer && !writeDone) {
          pending.push(writer.abort(error));
        }

        Promise.allSettled(pending).then((results) => {
          const rejected = results.find((result) => result.status === 'rejected');
          callback(error ?? rejected?.reason);
        });
      }
    });
    // Node option validation happens before acquiring locks. If acquisition of a
    // pair's second endpoint fails, restore the first rather than stranding it.
    try {
      if (readable) {
        reader = readable.getReader();
      }
      if (writable) {
        writer = writable.getWriter();
      }
    } catch (error) {
      reader?.releaseLock();
      throw error;
    }
    reader?.closed.catch((error) => node.destroy(error));
    writer?.closed.catch((error) => node.destroy(error));
    if (opts.signal !== undefined) {
      unsubscribe = abort.subscribe(opts.signal, () => {
        tick(() => node.destroy(abortFailure(abort.reason(opts.signal))));
      });
    }
    return node;
  }

  function nodeEndpoint(node, side) {
    const method = side === 'readable' ? 'read' : 'write';
    if (
      !node ||
      typeof node[method] !== 'function' ||
      typeof node.on !== 'function' ||
      typeof node.destroy !== 'function' ||
      (side === 'readable' ? !node._readableState : typeof node.end !== 'function')
    ) {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', `Expected a Node ${side} stream`);
    }
  }

  function terminalError(error) {
    return error?.code === 'ERR_STREAM_PREMATURE_CLOSE' ? abortFailure(error) : error;
  }

  function toReadable(node, value) {
    nodeEndpoint(node, 'readable');
    const opts = options(value);
    const strategy = opts.strategy ?? { highWaterMark: node.readableHighWaterMark };
    let controller;
    let cancelled = false;
    let terminal = false;
    let settle;
    const settled = new Promise((resolve) => {
      settle = resolve;
    });

    let cleanup = () => {};

    function data(chunk) {
      // Buffer storage can be pooled or reused by a producer. A Web Stream owns
      // its queued byte snapshot; object-mode values deliberately retain identity.
      if (!node.readableObjectMode && Buffer.isBuffer(chunk)) {
        chunk = new Uint8Array(chunk);
      }
      try {
        controller.enqueue(chunk);
        if (controller.desiredSize <= 0) {
          node.pause();
        }
      } catch (error) {
        node.destroy(error);
      }
    }

    return new ReadableStream(
      {
        start(value) {
          controller = value;
          if (node.destroyed || !node.readable) {
            terminal = true;
            controller.close();
            settle();
            return;
          }
          node.pause();
          node.on('data', data);
          cleanup = finished(node, { readable: true, writable: false }, (error) => {
            terminal = true;
            cleanup();
            node.removeListener('data', data);
            // The adapter owns failures after conversion, including late error
            // notifications from legacy sources after their terminal event.
            node.on('error', () => {});

            if (!cancelled) {
              if (error) {
                controller.error(terminalError(error));
              } else {
                controller.close();
              }
            }
            settle();
          });
        },

        pull() {
          if (!terminal) {
            node.resume();
          }
        },

        cancel(reason) {
          cancelled = true;
          if (!terminal) {
            node.destroy(reason);
          }
          return settled;
        }
      },
      strategy
    );
  }

  function toWritable(node) {
    nodeEndpoint(node, 'writable');
    if (node.destroyed || !node.writable) {
      const closed = new WritableStream();
      closed.close();
      return closed;
    }
    let controller;
    let terminal = false;
    let error;
    let aborted = false;
    let drainage;
    let settle;
    const settled = new Promise((resolve) => {
      settle = resolve;
    });

    let cleanup = () => {};

    function drain() {
      drainage?.resolve();
      drainage = undefined;
    }

    return new WritableStream(
      {
        start(value) {
          controller = value;
          node.on('drain', drain);
          cleanup = finished(node, { readable: false, writable: true }, (cause) => {
            terminal = true;
            error = terminalError(cause);
            cleanup();
            node.removeListener('drain', drain);

            node.on('error', () => {});

            if (error) {
              drainage?.reject(error);
            } else {
              drainage?.resolve();
            }
            drainage = undefined;
            if (error && !aborted) {
              controller.error(error);
            }
            settle();
          });
        },

        write(chunk) {
          if (terminal) {
            throw error ?? failure(Error, 'ERR_STREAM_DESTROYED', 'Destination is closed');
          }
          if (!node.write(chunk)) {
            return new Promise((resolve, reject) => {
              drainage = { resolve, reject };
            });
          }
        },

        close() {
          node.end();
          return settled.then(() => {
            if (error) {
              throw error;
            }
          });
        },

        abort(reason) {
          aborted = true;
          if (!terminal) {
            node.destroy(reason);
          }
          return settled;
        }
      },
      { highWaterMark: node.writableHighWaterMark }
    );
  }

  Readable.fromWeb = (readable, opts) => {
    webEndpoint(readable, readableLocked, 'readableStream');
    return fromWeb(readable, null, opts, Readable);
  };

  Readable.toWeb = toReadable;

  Writable.fromWeb = (writable, opts) => {
    webEndpoint(writable, writableLocked, 'writableStream');
    return fromWeb(null, writable, opts, Writable);
  };

  Writable.toWeb = toWritable;

  Duplex.fromWeb = (pair, opts) => {
    if (!pair || typeof pair !== 'object') {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', 'pair must contain readable and writable');
    }
    const { readable, writable } = pair;
    webEndpoint(readable, readableLocked, 'pair.readable');
    webEndpoint(writable, writableLocked, 'pair.writable');
    return fromWeb(readable, writable, opts, Duplex);
  };

  Duplex.toWeb = (node) => {
    nodeEndpoint(node, 'readable');
    nodeEndpoint(node, 'writable');
    return { readable: toReadable(node), writable: toWritable(node) };
  };
}
