/** Nonterminal readline over real Node-compatible streams; TTY editing is explicit. */
import createParser from './parser.mjs';
import createIterator from './iterator.mjs';

export default function createReadlineModule(host) {
  const { EventEmitter } = host.events;
  const { StringDecoder } = host.decoder;
  const states = new WeakMap();

  const unavailable = (name) => {
    throw Object.assign(new Error(`node:readline ${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  };

  const invalid = (message) => {
    throw Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  };

  class Interface extends EventEmitter {
    constructor(input, output, completer, terminal) {
      super();
      let options;
      if (input && typeof input === 'object' && 'input' in input) {
        options = input;
      } else {
        options = { input, output, completer, terminal };
      }
      for (const key of Object.keys(options)) {
        if (
          !['input', 'output', 'terminal', 'crlfDelay', 'prompt', 'signal', 'completer'].includes(
            key
          )
        ) {
          unavailable(`option ${key}`);
        }
      }
      input = options.input;
      output = options.output;
      if (
        !input ||
        typeof input.on !== 'function' ||
        typeof input.removeListener !== 'function' ||
        typeof input.resume !== 'function' ||
        typeof input.pause !== 'function'
      ) {
        invalid('input must be a readable stream');
      }
      if (output !== undefined && (!output || typeof output.write !== 'function')) {
        invalid('output must be a writable stream');
      }
      if (options.terminal ?? Boolean(output?.isTTY)) {
        unavailable('terminal editing');
      }
      if (options.completer !== undefined) {
        unavailable('completion');
      }
      const delay = options.crlfDelay === undefined ? 100 : options.crlfDelay;
      if (typeof delay !== 'number' || Number.isNaN(delay) || delay < 0) {
        invalid('crlfDelay must be a nonnegative number');
      }
      if (options.prompt !== undefined && typeof options.prompt !== 'string') {
        invalid('prompt must be a string');
      }
      if (options.signal !== undefined && !host.abort.isSignal(options.signal)) {
        invalid('signal must be an AbortSignal');
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
        iterator: undefined,
        prompt: options.prompt ?? '> ',
        question: undefined,

        unsubscribe: () => {}
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
            state.question = undefined;
            callback(line);
          } else {
            if (state.iterator) {
              keepGoing = state.iterator.push(line);
            }
            this.emit('line', line);
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

      input.on('data', state.data);
      input.on('end', state.end);
      input.on('error', state.error);
      state.unsubscribe =
        options.signal === undefined
          ? () => {}
          : host.abort.subscribe(options.signal, () => host.tick(() => this.close()));
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
        if (!state.iterator || this.listenerCount('error')) {
          this.emit('error', error);
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
        this.emit('pause');
      } else {
        this.emit('resume');
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
          state.deliveryError = undefined;
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
      state.input.removeListener('data', state.data);
      state.input.removeListener('end', state.end);
      state.input.removeListener('error', state.error);
      state.unsubscribe();
      state.question = undefined;
      if (!state.paused) {
        state.paused = true;
        state.input.pause();
        this.emit('pause');
      }
      state.iterator?.finish();
      this.emit('close');
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
      if (typeof value !== 'string') {
        invalid('prompt must be a string');
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
        throw Object.assign(new Error('readline was closed'), { code: 'ERR_USE_AFTER_CLOSE' });
      }
      if (typeof query !== 'string' || typeof callback !== 'function') {
        invalid('question needs a string and callback');
      }
      if (state.question) {
        return;
      }
      state.question = callback;
      state.output?.write(query);
      this.resume();
    }

    write() {
      unavailable('synthetic input and key events');
    }

    getCursorPos() {
      unavailable('terminal cursor position');
    }
  }
  return {
    Interface,

    createInterface: (...args) => new Interface(...args),

    clearLine: () => unavailable('clearLine'),

    clearScreenDown: () => unavailable('clearScreenDown'),

    cursorTo: () => unavailable('cursorTo'),

    moveCursor: () => unavailable('moveCursor'),

    emitKeypressEvents: () => unavailable('emitKeypressEvents')
  };
}
