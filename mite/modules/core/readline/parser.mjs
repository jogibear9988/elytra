/** Incremental UTF-8/CRLF framing; the interface owns delivery and read pressure. */
export default function createParser(Decoder, delay, emit) {
  const decoder = new Decoder('utf8');
  let text = '';
  let offset = 0;
  let parts = [];
  let length = 0;
  let crTime;
  let ended = false;

  function add(value) {
    length += value.length;
    if (length > 1048576) {
      throw Object.assign(new RangeError('Readline line exceeds one MiB of text'), {
        code: 'ERR_OUT_OF_RANGE'
      });
    }
    if (value) {
      parts.push(value);
    }
    if (parts.length >= 4096) {
      parts = [parts.join('')];
    }
  }

  function line() {
    const value = parts.join('');
    parts = [];
    length = 0;
    return emit(value);
  }

  return {
    write(chunk) {
      if (offset < text.length) {
        throw Object.assign(new Error('Input delivered while readline was backpressured'), {
          code: 'ERR_READLINE_INPUT_OVERFLOW'
        });
      }
      if (typeof chunk !== 'string' && !(chunk instanceof Uint8Array)) {
        throw Object.assign(new TypeError('readline input must contain UTF-8 bytes or strings'), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
      }
      if (chunk.length > 1048576) {
        throw Object.assign(new RangeError('Readline input chunk exceeds one MiB'), {
          code: 'ERR_OUT_OF_RANGE'
        });
      }
      text = typeof chunk === 'string' ? chunk : decoder.write(chunk);
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
        if (crTime !== undefined) {
          if (text[offset] === '\n' && Date.now() - crTime <= delay) {
            offset++;
          }
          crTime = undefined;
          start = offset;
        }
        while (offset < text.length && text[offset] !== '\n' && text[offset] !== '\r') {
          offset++;
        }
        add(text.slice(start, offset));
        if (offset < text.length) {
          const cr = text[offset++] === '\r';
          if (cr) {
            if (text[offset] === '\n') {
              offset++;
              crTime = undefined;
            } else {
              crTime = Date.now();
            }
          }
          if (!line()) {
            return false;
          }
        }
      }
      text = '';
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
