/** Stateful string decoding with Mite byte views and Node encoding aliases. */
import decoder from 'string_decoder/lib/string_decoder.js';
import { Buffer } from 'node:buffer';
const { StringDecoder: PortableDecoder } = decoder;

export default function createStringDecoderModule(host) {
  const states = new WeakMap();

  function error(code, message) {
    const value = new TypeError(message);
    value.code = code;
    return value;
  }

  function bytes(value) {
    if (typeof value === 'string') {
      return value;
    }
    if (!ArrayBuffer.isView(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'buffer must be a string or ArrayBufferView');
    }
    const view = host.byteView(value);
    return Buffer.from(view.buffer, view.byteOffset, view.byteLength);
  }

  function output(state, text) {
    return state.url ? text.replaceAll('+', '-').replaceAll('/', '_').replace(/=+$/, '') : text;
  }

  class StringDecoder {
    constructor(encoding) {
      if (encoding == null || encoding === '') {
        encoding = 'utf8';
      }
      if (typeof encoding !== 'string' || !Buffer.isEncoding(encoding)) {
        throw error('ERR_UNKNOWN_ENCODING', `Unknown encoding: ${String(encoding)}`);
      }
      encoding = encoding.toLowerCase();
      if (encoding === 'utf-8') {
        encoding = 'utf8';
      }
      if (['utf-16le', 'ucs-2', 'ucs2'].includes(encoding)) {
        encoding = 'utf16le';
      }
      if (encoding === 'binary') {
        encoding = 'latin1';
      }
      this.encoding = encoding;
      const codec = encoding === 'base64url' ? 'base64' : encoding;
      states.set(this, {
        url: encoding === 'base64url',
        codec,
        decoder: new PortableDecoder(codec)
      });
    }

    write(value) {
      const s = states.get(this);
      if (typeof value === 'string') {
        return value;
      }
      return output(s, s.decoder.write(bytes(value)));
    }

    end(value) {
      const s = states.get(this);
      const input = value === undefined ? undefined : bytes(value);
      const text =
        typeof value === 'string'
          ? value + output(s, s.decoder.end())
          : output(s, s.decoder.end(input));
      s.decoder = new PortableDecoder(s.codec);
      return text;
    }

    get lastNeed() {
      return states.get(this).decoder.lastNeed;
    }

    get lastTotal() {
      return states.get(this).decoder.lastTotal;
    }

    get lastChar() {
      return states.get(this).decoder.lastChar;
    }
  }
  return { StringDecoder };
}
