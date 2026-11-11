/*!
Copyright Joyent, Inc. and other Node contributors.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
/** Shared exact legacy percent encoder; provenance: vendor/node-url/manifest.json. */
export default function createQueryCodec() {
  const StringPrototypeCharCodeAt = (value, index) => value.charCodeAt(index);

  const StringPrototypeSlice = (value, start, end) => value.slice(start, end);

  const hexTable = Array.from(
    { length: 256 },
    (_, value) => '%' + value.toString(16).toUpperCase().padStart(2, '0')
  );
  const noEscapeQuery = Array.from({ length: 128 }, (_, value) =>
    /[A-Za-z0-9!\-._~'()*]/.test(String.fromCharCode(value)) ? 1 : 0
  );
  const noEscapeAuth = noEscapeQuery.slice();
  noEscapeAuth[58] = 1;
  class ERR_INVALID_URI extends URIError {
    constructor() {
      super('URI malformed');
      this.code = 'ERR_INVALID_URI';
    }
  }
  // Exact Node legacy encoder, including its code-unit behavior. It deliberately
  // differs from modern USVString conversion and encodeURIComponent for malformed UTF-16.
  function encodeStr(str, noEscapeTable, hexTable) {
    const len = str.length;
    if (len === 0) {
      return '';
    }

    let out = '';
    let lastPos = 0;
    let i = 0;

    outer: for (; i < len; i++) {
      let c = StringPrototypeCharCodeAt(str, i);

      // ASCII
      while (c < 0x80) {
        if (noEscapeTable[c] !== 1) {
          if (lastPos < i) {
            out += StringPrototypeSlice(str, lastPos, i);
          }
          lastPos = i + 1;
          out += hexTable[c];
        }

        if (++i === len) {
          break outer;
        }

        c = StringPrototypeCharCodeAt(str, i);
      }

      if (lastPos < i) {
        out += StringPrototypeSlice(str, lastPos, i);
      }

      // Multi-byte characters ...
      if (c < 0x800) {
        lastPos = i + 1;
        out += hexTable[0xc0 | (c >> 6)] + hexTable[0x80 | (c & 0x3f)];
        continue;
      }
      if (c < 0xd800 || c >= 0xe000) {
        lastPos = i + 1;
        out +=
          hexTable[0xe0 | (c >> 12)] +
          hexTable[0x80 | ((c >> 6) & 0x3f)] +
          hexTable[0x80 | (c & 0x3f)];
        continue;
      }
      // Surrogate pair
      ++i;

      // This branch should never happen because all URLSearchParams entries
      // should already be converted to USVString. But, included for
      // completion's sake anyway.
      if (i >= len) {
        throw new ERR_INVALID_URI();
      }

      const c2 = StringPrototypeCharCodeAt(str, i) & 0x3ff;

      lastPos = i + 1;
      c = 0x10000 + (((c & 0x3ff) << 10) | c2);
      out +=
        hexTable[0xf0 | (c >> 18)] +
        hexTable[0x80 | ((c >> 12) & 0x3f)] +
        hexTable[0x80 | ((c >> 6) & 0x3f)] +
        hexTable[0x80 | (c & 0x3f)];
    }
    if (lastPos === 0) {
      return str;
    }
    if (lastPos < len) {
      return out + StringPrototypeSlice(str, lastPos);
    }
    return out;
  }

  const isHexTable = Int8Array.from({ length: 256 }, (_, value) =>
    /[0-9A-Fa-f]/.test(String.fromCharCode(value)) ? 1 : 0
  );
  return { hexTable, noEscapeAuth, encodeStr, isHexTable };
}
