/* Generated from mite/modules/core/readline/index.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/readline/parser.mjs
function createParser(Decoder, delay, emit) {
  const decoder = new Decoder("utf8");
  let text = "";
  let offset = 0;
  let parts = [];
  let length = 0;
  let crTime;
  let ended = false;
  function add(value) {
    length += value.length;
    if (length > 1048576) {
      throw Object.assign(new RangeError("Readline line exceeds one MiB of text"), {
        code: "ERR_OUT_OF_RANGE"
      });
    }
    if (value) {
      parts.push(value);
    }
    if (parts.length >= 4096) {
      parts = [parts.join("")];
    }
  }
  function line() {
    const value = parts.join("");
    parts = [];
    length = 0;
    return emit(value);
  }
  return {
    write(chunk) {
      if (offset < text.length) {
        throw Object.assign(new Error("Input delivered while readline was backpressured"), {
          code: "ERR_READLINE_INPUT_OVERFLOW"
        });
      }
      if (typeof chunk !== "string" && !(chunk instanceof Uint8Array)) {
        throw Object.assign(new TypeError("readline input must contain UTF-8 bytes or strings"), {
          code: "ERR_INVALID_ARG_TYPE"
        });
      }
      if (chunk.length > 1048576) {
        throw Object.assign(new RangeError("Readline input chunk exceeds one MiB"), {
          code: "ERR_OUT_OF_RANGE"
        });
      }
      text = typeof chunk === "string" ? chunk : decoder.write(chunk);
      offset = 0;
    },
    // Like Node's nonterminal interface, do not turn an incomplete final
    // UTF-8 sequence into an additional replacement-character line.
    end() {
      ended = true;
    },
    pump() {
      while (offset < text.length) {
        let start = offset;
        if (crTime !== void 0) {
          if (text[offset] === "\n" && Date.now() - crTime <= delay) {
            offset++;
          }
          crTime = void 0;
          start = offset;
        }
        while (offset < text.length && text[offset] !== "\n" && text[offset] !== "\r") {
          offset++;
        }
        add(text.slice(start, offset));
        if (offset < text.length) {
          const cr = text[offset++] === "\r";
          if (cr) {
            if (text[offset] === "\n") {
              offset++;
              crTime = void 0;
            } else {
              crTime = Date.now();
            }
          }
          if (!line()) {
            return false;
          }
        }
      }
      text = "";
      offset = 0;
      if (ended && length) {
        if (!line()) {
          return false;
        }
      }
      return true;
    },
    get ended() {
      return ended;
    }
  };
}

// mite/modules/core/readline/iterator.mjs
function createIterator(rl, pause, resume) {
  let queue = [];
  let waiters = [];
  let done = false;
  let error;
  let failed = false;
  let paused = false;
  let size = 0;
  const settle = () => {
    while (waiters.length) {
      const waiter = waiters.shift();
      failed ? waiter.reject(error) : waiter.resolve({ done: true, value: void 0 });
    }
  };
  function drained() {
    if (paused && queue.length < 512 && size < 524288) {
      paused = false;
      resume();
    }
  }
  const iterator = {
    next() {
      if (queue.length) {
        const value = queue.shift();
        size -= value.length;
        drained();
        return Promise.resolve({ done: false, value });
      }
      if (failed) {
        return Promise.reject(error);
      }
      if (done) {
        return Promise.resolve({ done: true, value: void 0 });
      }
      return new Promise((resolve, reject) => waiters.push({ resolve, reject }));
    },
    return() {
      queue = [];
      size = 0;
      done = true;
      error = void 0;
      failed = false;
      settle();
      rl.close();
      return Promise.resolve({ done: true, value: void 0 });
    },
    throw(value) {
      queue = [];
      size = 0;
      done = true;
      error = value;
      failed = true;
      settle();
      rl.close();
      return Promise.reject(value);
    },
    [Symbol.asyncIterator]() {
      return this;
    }
  };
  return {
    iterator,
    push(value) {
      if (done) {
        return false;
      }
      if (waiters.length) {
        waiters.shift().resolve({ done: false, value });
      } else {
        queue.push(value);
        size += value.length;
      }
      if (queue.length >= 1024 || size >= 1048576) {
        paused = true;
        pause();
      }
      return !paused;
    },
    finish(...args) {
      done = true;
      if (args.length) {
        const value = args[0];
        error = value;
        failed = true;
        queue = [];
        size = 0;
      }
      settle();
    }
  };
}

// mite/modules/core/readline/index.mjs
function createReadlineModule(host) {
  const { EventEmitter } = host.events;
  const { StringDecoder } = host.decoder;
  const states = /* @__PURE__ */ new WeakMap();
  const unavailable = (name) => {
    throw Object.assign(new Error(`node:readline ${name} is not implemented`), {
      code: "ERR_MITE_UNSUPPORTED_API"
    });
  };
  const invalid = (message) => {
    throw Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  };
  class Interface extends EventEmitter {
    constructor(input, output, completer, terminal) {
      super();
      let options;
      if (input && typeof input === "object" && "input" in input) {
        options = input;
      } else {
        options = { input, output, completer, terminal };
      }
      for (const key of Object.keys(options)) {
        if (!["input", "output", "terminal", "crlfDelay", "prompt", "signal", "completer"].includes(
          key
        )) {
          unavailable(`option ${key}`);
        }
      }
      input = options.input;
      output = options.output;
      if (!input || typeof input.on !== "function" || typeof input.removeListener !== "function" || typeof input.resume !== "function" || typeof input.pause !== "function") {
        invalid("input must be a readable stream");
      }
      if (output !== void 0 && (!output || typeof output.write !== "function")) {
        invalid("output must be a writable stream");
      }
      if (options.terminal ?? Boolean(output?.isTTY)) {
        unavailable("terminal editing");
      }
      if (options.completer !== void 0) {
        unavailable("completion");
      }
      const delay = options.crlfDelay === void 0 ? 100 : options.crlfDelay;
      if (typeof delay !== "number" || Number.isNaN(delay) || delay < 0) {
        invalid("crlfDelay must be a nonnegative number");
      }
      if (options.prompt !== void 0 && typeof options.prompt !== "string") {
        invalid("prompt must be a string");
      }
      if (options.signal !== void 0 && !host.abort.isSignal(options.signal)) {
        invalid("signal must be an AbortSignal");
      }
      const state = {
        input,
        output,
        closed: false,
        manual: false,
        pressure: false,
        paused: false,
        draining: false,
        scheduled: false,
        iterator: void 0,
        prompt: options.prompt ?? "> ",
        question: void 0,
        unsubscribe: () => {
        }
      };
      states.set(this, state);
      this.input = input;
      this.output = output;
      this.terminal = false;
      state.parser = createParser(StringDecoder, Math.max(100, delay), (line) => {
        if (state.closed) {
          return false;
        }
        let keepGoing = true;
        try {
          if (state.question) {
            const callback = state.question;
            state.question = void 0;
            callback(line);
          } else {
            if (state.iterator) {
              keepGoing = state.iterator.push(line);
            }
            this.emit("line", line);
          }
          return !state.closed && keepGoing;
        } catch (error) {
          state.deliveryError = { value: error };
          throw error;
        }
      });
      state.data = (chunk) => {
        if (state.closed) {
          return;
        }
        try {
          state.parser.write(chunk);
        } catch (error) {
          this._failure(error);
          return;
        }
        this._drain();
      };
      state.end = () => {
        if (state.closed) {
          return;
        }
        try {
          state.parser.end();
        } catch (error) {
          this._failure(error);
          return;
        }
        this._drain();
      };
      state.error = (error) => this._failure(error);
      input.on("data", state.data);
      input.on("end", state.end);
      input.on("error", state.error);
      state.unsubscribe = options.signal === void 0 ? () => {
      } : host.abort.subscribe(options.signal, () => host.tick(() => this.close()));
      input.resume();
    }
    get closed() {
      return states.get(this).closed;
    }
    get paused() {
      return states.get(this).paused;
    }
    _failure(error) {
      const state = states.get(this);
      if (state.closed) {
        return;
      }
      if (state.iterator) {
        state.iterator.finish(error);
      }
      try {
        if (!state.iterator || this.listenerCount("error")) {
          this.emit("error", error);
        }
      } finally {
        this.close();
      }
    }
    _flow() {
      const state = states.get(this);
      const paused = state.manual || state.pressure;
      if (state.paused === paused) {
        return;
      }
      state.paused = paused;
      if (paused) {
        state.input.pause();
        this.emit("pause");
      } else {
        this.emit("resume");
        if (!state.closed && !state.manual && !state.pressure) {
          state.input.resume();
        }
      }
    }
    _drain() {
      const state = states.get(this);
      if (state.closed || state.draining || state.manual || state.pressure) {
        return;
      }
      state.draining = true;
      let complete;
      try {
        complete = state.parser.pump();
      } catch (error) {
        state.draining = false;
        if (state.deliveryError) {
          state.deliveryError = void 0;
          throw error;
        }
        this._failure(error);
        return;
      }
      state.draining = false;
      if (complete && state.parser.ended) {
        this.close();
      }
    }
    // Resume pending text in a later turn, preserving reentrant consumer pause.
    _resumeParser() {
      const state = states.get(this);
      if (state.scheduled || state.closed) {
        return;
      }
      state.scheduled = true;
      host.schedule(() => {
        state.scheduled = false;
        state.pressure = false;
        this._drain();
        if (!state.closed) {
          this._flow();
        }
      });
    }
    pause() {
      const state = states.get(this);
      if (!state.closed) {
        state.manual = true;
        this._flow();
      }
      return this;
    }
    resume() {
      const state = states.get(this);
      if (!state.closed) {
        state.manual = false;
        this._drain();
        this._flow();
      }
      return this;
    }
    // The interface releases listeners and pauses input; the caller owns the stream.
    close() {
      const state = states.get(this);
      if (state.closed) {
        return;
      }
      state.closed = true;
      state.input.removeListener("data", state.data);
      state.input.removeListener("end", state.end);
      state.input.removeListener("error", state.error);
      state.unsubscribe();
      state.question = void 0;
      if (!state.paused) {
        state.paused = true;
        state.input.pause();
        this.emit("pause");
      }
      state.iterator?.finish();
      this.emit("close");
    }
    [Symbol.asyncIterator]() {
      const state = states.get(this);
      if (!state.iterator) {
        state.iterator = createIterator(
          this,
          () => {
            state.pressure = true;
            this._flow();
          },
          () => this._resumeParser()
        );
        if (state.closed) {
          state.iterator.finish();
        }
      }
      return state.iterator.iterator;
    }
    getPrompt() {
      return states.get(this).prompt;
    }
    setPrompt(value) {
      if (typeof value !== "string") {
        invalid("prompt must be a string");
      }
      states.get(this).prompt = value;
    }
    prompt() {
      const state = states.get(this);
      if (!state.closed) {
        this.resume();
        state.output?.write(state.prompt);
      }
    }
    question(query, callback) {
      const state = states.get(this);
      if (state.closed) {
        throw Object.assign(new Error("readline was closed"), { code: "ERR_USE_AFTER_CLOSE" });
      }
      if (typeof query !== "string" || typeof callback !== "function") {
        invalid("question needs a string and callback");
      }
      if (state.question) {
        return;
      }
      state.question = callback;
      state.output?.write(query);
      this.resume();
    }
    write() {
      unavailable("synthetic input and key events");
    }
    getCursorPos() {
      unavailable("terminal cursor position");
    }
  }
  return {
    Interface,
    createInterface: (...args) => new Interface(...args),
    clearLine: () => unavailable("clearLine"),
    clearScreenDown: () => unavailable("clearScreenDown"),
    cursorTo: () => unavailable("cursorTo"),
    moveCursor: () => unavailable("moveCursor"),
    emitKeypressEvents: () => unavailable("emitKeypressEvents")
  };
}
return createReadlineModule;
})()
