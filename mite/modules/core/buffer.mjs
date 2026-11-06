/**
 * Node-compatible byte buffers on LibJS ArrayBuffer storage. Validate byte ranges before
 * access and retain callable Buffer construction for compatibility.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createBufferModule() {
  const MAX_LENGTH = 0x7fffffff;

  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }

  function size(value) {
    if (typeof value !== 'number') {
      fail('ERR_INVALID_ARG_TYPE', 'size must be a number');
    }
    if (!Number.isFinite(value) || value < 0 || value > MAX_LENGTH) {
      fail('ERR_OUT_OF_RANGE', 'size is outside the supported Buffer range', RangeError);
    }
    return Math.trunc(value);
  }

  /** Normalize supported Node encoding aliases without reading outside a byte view. */
  function encoding(value, strict = true) {
    if (value === undefined) {
      return 'utf8';
    }
    if (typeof value !== 'string') {
      if (!strict) {
        return 'utf8';
      }
      fail('ERR_UNKNOWN_ENCODING', `Unknown encoding: ${String(value)}`);
    }
    switch (value.toLowerCase()) {
      case 'utf8':
      case 'utf-8':
        return 'utf8';
      case 'utf16le':
      case 'utf-16le':
      case 'ucs2':
      case 'ucs-2':
        return 'utf16le';
      case 'latin1':
      case 'binary':
        return 'latin1';
      case 'ascii':
      case 'hex':
      case 'base64':
      case 'base64url':
        return value.toLowerCase();
    }
    if (strict) {
      fail('ERR_UNKNOWN_ENCODING', `Unknown encoding: ${value}`);
    }
    return 'utf8';
  }

  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

  function encode(text, codec) {
    const bytes = [];
    if (codec === 'hex') {
      for (let i = 0; i + 1 < text.length; i += 2) {
        const pair = text.slice(i, i + 2);
        if (!/^[0-9a-f]{2}$/i.test(pair)) {
          break;
        }
        bytes.push(parseInt(pair, 16));
      }
    } else if (codec === 'base64' || codec === 'base64url') {
      let bits = 0;
      let count = 0;
      for (let i = 0; i < text.length; ++i) {
        // Node's forgiving decoder uses the low byte of UTF-16 input.
        const character = String.fromCharCode(text.charCodeAt(i) & 255);
        if (character === '=') {
          break;
        }
        const value = alphabet.indexOf(
          character === '-' ? '+' : character === '_' ? '/' : character
        );
        if (value < 0) {
          continue;
        }
        bits = (bits << 6) | value;
        count += 6;
        if (count >= 8) {
          count -= 8;
          bytes.push((bits >>> count) & 255);
        }
      }
    } else {
      for (let i = 0; i < text.length; ++i) {
        let point = text.charCodeAt(i);
        if (codec === 'latin1' || codec === 'ascii') {
          bytes.push(point & 255);
          continue;
        }
        if (codec === 'utf16le') {
          bytes.push(point & 255, point >>> 8);
          continue;
        }
        if (point >= 0xd800 && point <= 0xdbff) {
          const next = text.charCodeAt(i + 1);
          if (next >= 0xdc00 && next <= 0xdfff) {
            point = 0x10000 + ((point - 0xd800) << 10) + next - 0xdc00;
            ++i;
          } else {
            point = 0xfffd;
          }
        } else if (point >= 0xdc00 && point <= 0xdfff) {
          point = 0xfffd;
        }
        if (point < 0x80) {
          bytes.push(point);
        } else if (point < 0x800) {
          bytes.push(0xc0 | (point >> 6), 0x80 | (point & 63));
        } else if (point < 0x10000) {
          bytes.push(0xe0 | (point >> 12), 0x80 | ((point >> 6) & 63), 0x80 | (point & 63));
        } else {
          bytes.push(
            0xf0 | (point >> 18),
            0x80 | ((point >> 12) & 63),
            0x80 | ((point >> 6) & 63),
            0x80 | (point & 63)
          );
        }
      }
    }
    return bytes;
  }

  function decode(bytes, codec) {
    let result = '';
    if (codec === 'hex') {
      for (const value of bytes) {
        result += value.toString(16).padStart(2, '0');
      }
    } else if (codec === 'base64' || codec === 'base64url') {
      for (let i = 0; i < bytes.length; i += 3) {
        const a = bytes[i];
        const b = bytes[i + 1];
        const c = bytes[i + 2];
        result +=
          alphabet[a >> 2] +
          alphabet[((a & 3) << 4) | ((b || 0) >> 4)] +
          (i + 1 < bytes.length ? alphabet[((b & 15) << 2) | ((c || 0) >> 6)] : '=') +
          (i + 2 < bytes.length ? alphabet[c & 63] : '=');
      }
      if (codec === 'base64url') {
        result = result.replaceAll('+', '-').replaceAll('/', '_').replace(/=+$/, '');
      }
    } else if (codec === 'utf16le') {
      for (let i = 0; i + 1 < bytes.length; i += 2) {
        result += String.fromCharCode(bytes[i] | (bytes[i + 1] << 8));
      }
    } else if (codec === 'ascii' || codec === 'latin1') {
      for (const value of bytes) {
        result += String.fromCharCode(value & (codec === 'ascii' ? 127 : 255));
      }
    } else {
      for (let i = 0; i < bytes.length; ) {
        const first = bytes[i++];
        if (first < 0x80) {
          result += String.fromCharCode(first);
          continue;
        }
        let remaining;
        let point;
        let lower = 0x80;
        let upper = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
          remaining = 1;
          point = first & 31;
        } else if (first >= 0xe0 && first <= 0xef) {
          remaining = 2;
          point = first & 15;
          if (first === 0xe0) {
            lower = 0xa0;
          }
          if (first === 0xed) {
            upper = 0x9f;
          }
        } else if (first >= 0xf0 && first <= 0xf4) {
          remaining = 3;
          point = first & 7;
          if (first === 0xf0) {
            lower = 0x90;
          }
          if (first === 0xf4) {
            upper = 0x8f;
          }
        } else {
          result += '\ufffd';
          continue;
        }
        let valid = true;
        while (remaining--) {
          const next = bytes[i];
          if (next === undefined || next < lower || next > upper) {
            valid = false;
            break;
          }
          point = (point << 6) | (next & 63);
          ++i;
          lower = 0x80;
          upper = 0xbf;
        }
        result += valid ? String.fromCodePoint(point) : '\ufffd';
      }
    }
    return result;
  }

  class BufferView extends Uint8Array {}

  /** Retain callable construction while returning correctly branded Uint8Array-backed buffers. */
  function Buffer(value, offsetOrEncoding, length) {
    return typeof value === 'number'
      ? Buffer.alloc(value)
      : Buffer.from(value, offsetOrEncoding, length);
  }

  Object.setPrototypeOf(Buffer, Uint8Array);
  Buffer.prototype = BufferView.prototype;
  Object.defineProperty(Buffer.prototype, 'constructor', {
    value: Buffer,
    writable: true,
    configurable: true
  });
  Object.defineProperty(Buffer, Symbol.species, { value: BufferView });

  function bytes(value) {
    if (!(value instanceof Uint8Array)) {
      fail('ERR_INVALID_ARG_TYPE', 'Expected a Buffer or Uint8Array');
    }
    return value;
  }

  Buffer.from = function from(value, offsetOrEncoding, length) {
    if (typeof value === 'string') {
      const data = encode(
        value,
        encoding(
          typeof offsetOrEncoding === 'string' && offsetOrEncoding ? offsetOrEncoding : undefined
        )
      );
      size(data.length);
      return new BufferView(data);
    }
    if (value instanceof ArrayBuffer || value instanceof SharedArrayBuffer) {
      const offset = offsetOrEncoding === undefined ? 0 : Number(offsetOrEncoding);
      if (offset > value.byteLength) {
        fail('ERR_BUFFER_OUT_OF_BOUNDS', 'offset is outside the buffer', RangeError);
      }
      if (length !== undefined && Number(length) > value.byteLength - (offset || 0)) {
        fail('ERR_BUFFER_OUT_OF_BOUNDS', 'length is outside the buffer', RangeError);
      }
      return length === undefined
        ? new BufferView(value, offset)
        : new BufferView(value, offset, Math.max(0, Number(length) || 0));
    }
    if (value !== null && typeof value === 'object') {
      const primitive = typeof value.valueOf === 'function' ? value.valueOf() : value;
      if (primitive !== value && (typeof primitive === 'string' || typeof primitive === 'object')) {
        return Buffer.from(primitive, offsetOrEncoding, length);
      }
      if (ArrayBuffer.isView(value) && !(value instanceof DataView)) {
        size(Math.max(0, value.length));
        return new BufferView(value);
      }
      if (value.type === 'Buffer' && Array.isArray(value.data)) {
        return Buffer.from(value.data);
      }
      if (ArrayBuffer.isView(value) || 'length' in value) {
        if (typeof value.length !== 'number' || Number.isNaN(value.length)) {
          return new BufferView(0);
        }
        const result = new BufferView(size(Math.max(0, value.length)));
        for (let i = 0; i < result.length; ++i) {
          result[i] = value[i];
        }
        return result;
      }
      if (typeof value[Symbol.toPrimitive] === 'function') {
        return Buffer.from(value[Symbol.toPrimitive]('string'), offsetOrEncoding, length);
      }
    }
    fail(
      'ERR_INVALID_ARG_TYPE',
      'First argument must be a string, Buffer, ArrayBuffer, Array, or array-like object'
    );
  };

  Buffer.alloc = function alloc(length, fill, codec) {
    const result = new BufferView(size(length));
    if (fill !== undefined && result.length) {
      result.fill(fill, 0, result.length, codec);
    }
    return result;
  };
  // Fresh zeroed backing stores deliberately avoid exposing uninitialized data.
  Buffer.allocUnsafe = function allocUnsafe(length) {
    return new BufferView(size(length));
  };

  Buffer.allocUnsafeSlow = function allocUnsafeSlow(length) {
    return new BufferView(size(length));
  };

  Buffer.isBuffer = function isBuffer(value) {
    return value instanceof Buffer;
  };

  Buffer.isEncoding = function isEncoding(value) {
    if (typeof value !== 'string' || !value) {
      return false;
    }
    try {
      encoding(value);
      return true;
    } catch {
      return false;
    }
  };

  Buffer.byteLength = function byteLength(value, codec) {
    if (ArrayBuffer.isView(value)) {
      return value.byteLength;
    }
    if (value instanceof ArrayBuffer || value instanceof SharedArrayBuffer) {
      return value.byteLength;
    }
    if (typeof value !== 'string') {
      fail('ERR_INVALID_ARG_TYPE', 'string must be a string or buffer');
    }
    codec = encoding(codec, false);
    if (codec === 'hex') {
      return value.length >>> 1;
    }
    if (codec === 'base64' || codec === 'base64url') {
      return Math.floor((value.replace(/={1,2}$/, '').length * 3) / 4);
    }
    return encode(value, codec).length;
  };

  Buffer.compare = function compare(a, b) {
    bytes(a);
    bytes(b);
    const length = Math.min(a.length, b.length);
    for (let i = 0; i < length; ++i) {
      if (a[i] !== b[i]) {
        return a[i] < b[i] ? -1 : 1;
      }
    }
    return a.length === b.length ? 0 : a.length < b.length ? -1 : 1;
  };

  Buffer.concat = function concat(list, totalLength) {
    if (!Array.isArray(list)) {
      fail('ERR_INVALID_ARG_TYPE', 'list must be an Array');
    }
    let length = 0;
    for (const entry of list) {
      length += bytes(entry).length;
    }
    const result = Buffer.alloc(totalLength === undefined ? length : size(totalLength));
    let offset = 0;
    for (const entry of list) {
      const count = Math.min(entry.length, result.length - offset);
      result.set(entry.subarray(0, count), offset);
      offset += count;
    }
    return result;
  };

  Buffer.copyBytesFrom = function copyBytesFrom(view, offset = 0, length) {
    if (!ArrayBuffer.isView(view) || view instanceof DataView) {
      fail('ERR_INVALID_ARG_TYPE', 'view must be a TypedArray');
    }
    integer(offset, 0, Number.MAX_SAFE_INTEGER, 'offset');
    if (length !== undefined) {
      integer(length, 0, Number.MAX_SAFE_INTEGER, 'length');
    }
    const count = Math.min(
      length === undefined ? view.length : length,
      Math.max(0, view.length - offset)
    );
    return Buffer.from(
      new Uint8Array(
        view.buffer,
        view.byteOffset + Math.min(offset, view.length) * view.BYTES_PER_ELEMENT,
        count * view.BYTES_PER_ELEMENT
      )
    );
  };

  function integer(value, minimum, maximum, name) {
    if (typeof value !== 'number') {
      fail('ERR_INVALID_ARG_TYPE', `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail('ERR_OUT_OF_RANGE', `${name} is out of range`, RangeError);
    }
    return value;
  }

  Buffer.prototype.toString = function toString(codec, start = 0, end = this.length) {
    if (!this.length) {
      return '';
    }
    start = +start;
    end = +end;
    if (start < 0 || Number.isNaN(start)) {
      start = 0;
    }
    if (end > this.length) {
      end = this.length;
    }
    if (start >= this.length || end <= start || end <= 0) {
      return '';
    }
    start >>>= 0;
    end >>>= 0;
    return decode(Uint8Array.prototype.subarray.call(this, start, end), encoding(codec));
  };

  Buffer.prototype.toJSON = function toJSON() {
    return { type: 'Buffer', data: Array.from(this) };
  };

  // Multipart libraries use Node's direct codec slices rather than toString.
  // These share decoding, but validate bounds instead of toString's clamping.
  const stringify = Buffer.prototype.toString;
  for (const [name, codec] of Object.entries({
    utf8Slice: 'utf8',
    latin1Slice: 'latin1',
    ucs2Slice: 'utf16le',
    base64Slice: 'base64'
  })) {
    Buffer.prototype[name] = function (start = 0, end = this.length) {
      start = Math.trunc(+start) || 0;
      end = Math.trunc(+end) || 0;
      if (start < 0 || start > this.length || end < 0 || end > this.length) {
        fail('ERR_OUT_OF_RANGE', 'Codec slice bounds are outside the buffer', RangeError);
      }
      return stringify.call(this, codec, start, end);
    };
  }

  Buffer.prototype.slice = function slice(start, end) {
    return this.subarray(start, end);
  };

  Buffer.prototype.equals = function equals(other) {
    return Buffer.compare(this, bytes(other)) === 0;
  };

  Buffer.prototype.compare = function compare(
    other,
    targetStart = 0,
    targetEnd = other.length,
    sourceStart = 0,
    sourceEnd = this.length
  ) {
    bytes(other);
    integer(targetStart, 0, Number.MAX_SAFE_INTEGER, 'targetStart');
    integer(targetEnd, 0, other.length, 'targetEnd');
    integer(sourceStart, 0, Number.MAX_SAFE_INTEGER, 'sourceStart');
    integer(sourceEnd, 0, this.length, 'sourceEnd');
    return Buffer.compare(
      this.subarray(sourceStart, sourceEnd),
      other.subarray(targetStart, targetEnd)
    );
  };

  Buffer.prototype.copy = function copy(
    target,
    targetStart = 0,
    sourceStart = 0,
    sourceEnd = this.length
  ) {
    bytes(target);
    targetStart = Math.trunc(+targetStart) || 0;
    sourceStart = Math.trunc(+sourceStart) || 0;
    sourceEnd = Math.trunc(+sourceEnd) || 0;
    if (targetStart < 0 || sourceStart < 0 || sourceStart > this.length || sourceEnd < 0) {
      fail('ERR_OUT_OF_RANGE', 'copy bounds are out of range', RangeError);
    }
    const count = Math.max(0, Math.min(sourceEnd, this.length) - sourceStart);
    const copied = Math.min(count, Math.max(0, target.length - targetStart));
    // TypedArray.set supplies memmove semantics even for overlapping views.
    if (copied) {
      target.set(this.subarray(sourceStart, sourceStart + copied), targetStart);
    }
    return copied;
  };

  Buffer.prototype.fill = function fill(value, offset = 0, end = this.length, codec) {
    if (typeof offset === 'string') {
      codec = offset;
      offset = 0;
      end = this.length;
    } else if (typeof end === 'string') {
      codec = end;
      end = this.length;
    }
    integer(offset, 0, Number.MAX_SAFE_INTEGER, 'offset');
    integer(end, 0, this.length, 'end');
    if (typeof value !== 'string' && !(value instanceof Uint8Array)) {
      return Uint8Array.prototype.fill.call(this, value, offset, end);
    }
    if (typeof value === 'string' && codec != null && typeof codec !== 'string') {
      fail('ERR_INVALID_ARG_TYPE', 'encoding must be a string');
    }
    const pattern = typeof value === 'string' ? encode(value, encoding(codec || undefined)) : value;
    if (end <= offset) {
      return this;
    }
    if (!pattern.length) {
      if (value === '') {
        return Uint8Array.prototype.fill.call(this, 0, offset, end);
      }
      fail('ERR_INVALID_ARG_VALUE', 'fill pattern is empty');
    }
    // Snapshot first: the fill pattern may alias the destination.
    const copy = Array.from(pattern);
    for (let i = offset; i < end; ++i) {
      this[i] = copy[(i - offset) % copy.length];
    }
    return this;
  };

  Buffer.prototype.write = function write(text, offset = 0, length, codec) {
    if (typeof text !== 'string') {
      fail('ERR_INVALID_ARG_TYPE', 'string must be a string');
    }
    if (typeof offset === 'string') {
      codec = offset;
      offset = 0;
      length = this.length;
    } else if (typeof length === 'string') {
      codec = length;
      length = undefined;
    }
    integer(offset, 0, this.length, 'offset');
    if (length === undefined) {
      length = this.length - offset;
    }
    integer(length, 0, this.length, 'length');
    length = Math.min(length, this.length - offset);
    codec = encoding(codec || undefined);
    const data = encode(text, codec);
    let count = Math.min(length, data.length);
    if (codec === 'utf16le') {
      count -= count % 2;
    }
    if (codec === 'utf8' && count < data.length) {
      let boundary = count;
      while (boundary > 0 && (data[boundary] & 0xc0) === 0x80) {
        --boundary;
      }
      count = boundary;
    }
    this.set(data.slice(0, count), offset);
    return count;
  };

  function search(buffer, value, offset, codec, backwards) {
    if (typeof offset === 'string') {
      codec = offset;
      offset = undefined;
    }
    const numeric = typeof value === 'number';
    const pattern = numeric
      ? [value & 255]
      : typeof value === 'string'
        ? encode(value, encoding(codec))
        : bytes(value);
    const wide = !numeric && typeof codec === 'string' && encoding(codec) === 'utf16le';
    if (offset === undefined || Number.isNaN(+offset)) {
      offset = backwards ? (pattern.length ? buffer.length - 1 : buffer.length) : 0;
    } else {
      offset = Math.trunc(Math.max(-0x80000000, Math.min(0x7fffffff, +offset)));
    }
    if (offset < 0) {
      offset += buffer.length;
    }
    if (!pattern.length) {
      return Math.max(0, Math.min(buffer.length, offset));
    }
    if (backwards) {
      if (offset < 0) {
        return -1;
      }
      offset = Math.min(offset, buffer.length - pattern.length);
    } else {
      offset = Math.max(0, offset);
    }
    const step = wide ? 2 : 1;
    const patternLength = wide ? pattern.length - (pattern.length % 2) : pattern.length;
    if (!patternLength || (wide && buffer.length < 2)) {
      return -1;
    }
    if (!backwards && offset + patternLength > buffer.length) {
      return -1;
    }
    if (wide) {
      offset -= offset % 2;
    }
    for (
      let i = offset;
      backwards ? i >= 0 : i + patternLength <= buffer.length;
      i += backwards ? -step : step
    ) {
      if (i + patternLength > buffer.length) {
        continue;
      }
      let j = 0;
      while (j < patternLength && buffer[i + j] === pattern[j]) {
        ++j;
      }
      if (j === patternLength) {
        return i;
      }
    }
    return -1;
  }

  Buffer.prototype.indexOf = function indexOf(value, offset, codec) {
    return search(this, value, offset, codec, false);
  };

  Buffer.prototype.lastIndexOf = function lastIndexOf(value, offset, codec) {
    return search(this, value, offset, codec, true);
  };

  Buffer.prototype.includes = function includes(value, offset, codec) {
    return this.indexOf(value, offset, codec) !== -1;
  };

  for (const width of [2, 4, 8]) {
    Buffer.prototype['swap' + width * 8] = function () {
      if (this.length % width) {
        fail(
          'ERR_INVALID_BUFFER_SIZE',
          'Buffer size must be a multiple of the word size',
          RangeError
        );
      }
      for (let i = 0; i < this.length; i += width) {
        for (let j = 0; j < width / 2; ++j) {
          const saved = this[i + j];
          this[i + j] = this[i + width - j - 1];
          this[i + width - j - 1] = saved;
        }
      }
      return this;
    };
  }

  function bounds(buffer, offset, width) {
    if (typeof offset !== 'number') {
      fail('ERR_INVALID_ARG_TYPE', 'offset must be a number');
    }
    if (!Number.isInteger(offset)) {
      fail('ERR_OUT_OF_RANGE', 'offset must be an integer', RangeError);
    }
    if (offset < 0 || offset + width > buffer.length) {
      if (buffer.length < width) {
        fail(
          'ERR_BUFFER_OUT_OF_BOUNDS',
          'Attempt to access memory outside buffer bounds',
          RangeError
        );
      }
      fail('ERR_OUT_OF_RANGE', 'offset is outside the buffer', RangeError);
    }
  }

  function readInteger(buffer, offset, width, little, signed) {
    bounds(buffer, offset, width);
    let result = 0;
    for (let i = 0; i < width; ++i) {
      result = result * 256 + buffer[offset + (little ? width - i - 1 : i)];
    }
    const sign = 2 ** (width * 8 - 1);
    return signed && result >= sign ? result - 2 ** (width * 8) : result;
  }

  function writeInteger(buffer, value, offset, width, little, signed) {
    value = +value;
    const sign = 2 ** (width * 8 - (signed ? 1 : 0));
    if (value < (signed ? -sign : 0) || value > sign - 1) {
      fail('ERR_OUT_OF_RANGE', 'value is outside the integer range', RangeError);
    }
    bounds(buffer, offset, width);
    value = Math.trunc(value) || 0;
    if (value < 0) {
      value += 2 ** (width * 8);
    }
    for (let i = 0; i < width; ++i) {
      buffer[offset + (little ? i : width - i - 1)] = value % 256;
      value = Math.floor(value / 256);
    }
    return offset + width;
  }

  for (const signed of [false, true]) {
    const kind = signed ? 'Int' : 'UInt';
    for (const little of [false, true]) {
      const order = little ? 'LE' : 'BE';

      Buffer.prototype['read' + kind + order] = function (offset, byteLength) {
        integer(byteLength, 1, 6, 'byteLength');
        return readInteger(this, offset, byteLength, little, signed);
      };

      Buffer.prototype['write' + kind + order] = function (value, offset, byteLength) {
        integer(byteLength, 1, 6, 'byteLength');
        return writeInteger(this, value, offset, byteLength, little, signed);
      };

      for (const width of [1, 2, 4]) {
        if (width === 1 && little) {
          continue;
        }
        const suffix = kind + width * 8 + (width === 1 ? '' : order);

        Buffer.prototype['read' + suffix] = function (offset = 0) {
          return readInteger(this, offset, width, little, signed);
        };

        Buffer.prototype['write' + suffix] = function (value, offset = 0) {
          return writeInteger(this, value, offset, width, little, signed);
        };
      }
    }
  }
  for (const [kind, width] of [
    ['Float', 4],
    ['Double', 8]
  ]) {
    for (const little of [false, true]) {
      const suffix = kind + (little ? 'LE' : 'BE');

      Buffer.prototype['read' + suffix] = function (offset = 0) {
        bounds(this, offset, width);
        return new DataView(this.buffer, this.byteOffset, this.byteLength)['getFloat' + width * 8](
          offset,
          little
        );
      };

      Buffer.prototype['write' + suffix] = function (value, offset = 0) {
        value = +value;
        bounds(this, offset, width);
        new DataView(this.buffer, this.byteOffset, this.byteLength)['setFloat' + width * 8](
          offset,
          value,
          little
        );
        return offset + width;
      };
    }
  }
  for (const signed of [false, true]) {
    for (const little of [false, true]) {
      const suffix = (signed ? 'BigInt64' : 'BigUInt64') + (little ? 'LE' : 'BE');

      Buffer.prototype['read' + suffix] = function (offset = 0) {
        bounds(this, offset, 8);
        let result = 0n;
        for (let i = 0; i < 8; ++i) {
          result = (result << 8n) | BigInt(this[offset + (little ? 7 - i : i)]);
        }
        return signed ? BigInt.asIntN(64, result) : result;
      };

      Buffer.prototype['write' + suffix] = function (value, offset = 0) {
        const limit = 1n << (signed ? 63n : 64n);
        if (value < (signed ? -limit : 0n) || value >= limit) {
          fail('ERR_OUT_OF_RANGE', 'value is outside the bigint range', RangeError);
        }
        bounds(this, offset, 8);
        if (typeof value !== 'bigint') {
          throw new TypeError('value must be a bigint');
        }
        value = BigInt.asUintN(64, value);
        for (let i = 0; i < 8; ++i) {
          this[offset + (little ? i : 7 - i)] = Number(value & 255n);
          value >>= 8n;
        }
        return offset + 8;
      };
    }
  }
  // Node exposes both UInt and Uint spellings as the same functions.
  for (const key of Object.keys(Buffer.prototype)) {
    if (key.includes('UInt')) {
      Buffer.prototype[key.replace('UInt', 'Uint')] = Buffer.prototype[key];
    }
  }
  Object.defineProperty(Buffer.prototype, 'parent', {
    enumerable: true,
    configurable: true,

    get() {
      return Buffer.isBuffer(this) ? this.buffer : undefined;
    }
  });
  Object.defineProperty(Buffer.prototype, 'offset', {
    enumerable: true,
    configurable: true,

    get() {
      return Buffer.isBuffer(this) ? this.byteOffset : undefined;
    }
  });

  function SlowBuffer(length) {
    return Buffer.alloc(+length || 0);
  }

  return { Buffer, SlowBuffer };
}
