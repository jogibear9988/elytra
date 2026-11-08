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
/**
 * Maintained ESM adaptation of Node v24.15.0 legacy parsing/formatting.
 * Exact upstream sources and hashes: vendor/node-url/manifest.json.
 * Legacy parsing deliberately preserves ports, relative references and numeric
 * host spellings that WHATWG URL canonicalizes. Node's deprecation diagnostics
 * and its unrelated URL resolution API are outside this slice.
 */
export default function createLegacyUrl(native) {
  const StringPrototypeCharCodeAt = (value, index) => value.charCodeAt(index);

  const StringPrototypeIndexOf = (value, search) => value.indexOf(search);

  const StringPrototypeReplaceAll = (value, search, replacement) =>
    value.replaceAll(search, replacement);

  const StringPrototypeSlice = (value, start, end) => value.slice(start, end);

  const toASCII = (value) => native.asciiDomain(value);

  const unsafeProtocol = new Set(['javascript', 'javascript:']);
  const hostlessProtocol = new Set(['javascript', 'javascript:']);
  const slashedProtocol = new Set(
    ['http', 'https', 'ftp', 'gopher', 'file', 'ws', 'wss'].flatMap((value) => [value, value + ':'])
  );
  class ERR_INVALID_URL extends TypeError {
    constructor(input) {
      super('Invalid URL');
      this.code = 'ERR_INVALID_URL';
      this.input = input;
    }
  }

  function validateString(value) {
    if (typeof value !== 'string') {
      throw Object.assign(new TypeError('url must be a string'), { code: 'ERR_INVALID_ARG_TYPE' });
    }
  }

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

  function encodeAuth(value) {
    return encodeStr(value, noEscapeAuth, hexTable);
  }

  function decodeQuery(value) {
    value = value.replaceAll('+', ' ');
    if (!/%[0-9a-f]{2}/i.test(value)) {
      return value;
    }
    try {
      return decodeURIComponent(value);
    } catch {
      // Node querystring's unescapeBuffer fallback retains each undecoded UTF-16
      // code unit's low byte, then performs forgiving UTF-8 decoding. Encode that
      // exact byte sequence before using the native form decoder solely as UTF-8 decoder.
      const bytes = [];
      for (let index = 0; index < value.length; ++index) {
        const pair = value.slice(index + 1, index + 3);
        if (value[index] === '%' && /^[0-9a-f]{2}$/i.test(pair)) {
          bytes.push(hexTable[parseInt(pair, 16)]);
          index += 2;
        } else {
          bytes.push(hexTable[value.charCodeAt(index) & 255]);
        }
      }
      return native.decodeForm('value=' + bytes.join(''))[0]?.[1] ?? '';
    }
  }

  const querystring = {
    parse(value) {
      const result = Object.create(null);
      // querystring's 1000-key budget counts empty '&' segments too. Bound
      // the source before forgiving form decoding rather than trimming its pairs.
      let end = -1;
      for (let count = 0; count < 1000; ++count) {
        end = value.indexOf('&', end + 1);
        if (end === -1) {
          break;
        }
      }
      const selected = end === -1 ? value : value.slice(0, end);
      for (const pair of selected.split('&')) {
        if (!pair) {
          continue;
        }
        const equal = pair.indexOf('=');
        const name = decodeQuery(equal < 0 ? pair : pair.slice(0, equal));
        const item = decodeQuery(equal < 0 ? '' : pair.slice(equal + 1));
        if (!Object.hasOwn(result, name)) {
          result[name] = item;
        } else if (Array.isArray(result[name])) {
          result[name].push(item);
        } else {
          result[name] = [result[name], item];
        }
      }
      return result;
    },

    stringify(value) {
      const pairs = [];

      const scalar = (item) =>
        typeof item === 'string' ||
        typeof item === 'boolean' ||
        (typeof item === 'number' && Number.isFinite(item)) ||
        typeof item === 'bigint'
          ? String(item)
          : '';

      for (const name of Object.keys(value)) {
        const items = Array.isArray(value[name]) ? value[name] : [value[name]];
        for (const item of items) {
          pairs.push(
            encodeStr(name, noEscapeQuery, hexTable) +
              '=' +
              encodeStr(scalar(item), noEscapeQuery, hexTable)
          );
        }
      }
      return pairs.join('&');
    }
  };
  const CHAR_SPACE = 32;
  const CHAR_TAB = 9;
  const CHAR_CARRIAGE_RETURN = 13;
  const CHAR_LINE_FEED = 10;
  const CHAR_NO_BREAK_SPACE = 160;
  const CHAR_ZERO_WIDTH_NOBREAK_SPACE = 65279;
  const CHAR_HASH = 35;
  const CHAR_FORWARD_SLASH = 47;
  const CHAR_LEFT_SQUARE_BRACKET = 91;
  const CHAR_RIGHT_SQUARE_BRACKET = 93;
  const CHAR_LEFT_ANGLE_BRACKET = 60;
  const CHAR_RIGHT_ANGLE_BRACKET = 62;
  const CHAR_LEFT_CURLY_BRACKET = 123;
  const CHAR_RIGHT_CURLY_BRACKET = 125;
  const CHAR_QUESTION_MARK = 63;
  const CHAR_DOUBLE_QUOTE = 34;
  const CHAR_SINGLE_QUOTE = 39;
  const CHAR_PERCENT = 37;
  const CHAR_SEMICOLON = 59;
  const CHAR_BACKWARD_SLASH = 92;
  const CHAR_CIRCUMFLEX_ACCENT = 94;
  const CHAR_GRAVE_ACCENT = 96;
  const CHAR_VERTICAL_LINE = 124;
  const CHAR_AT = 64;
  const CHAR_COLON = 58;

  function Url() {
    this.protocol = null;
    this.slashes = null;
    this.auth = null;
    this.host = null;
    this.port = null;
    this.hostname = null;
    this.hash = null;
    this.search = null;
    this.query = null;
    this.pathname = null;
    this.path = null;
    this.href = null;
  }

  // Reference: RFC 3986, RFC 1808, RFC 2396

  // define these here so at least they only have to be
  // compiled once on the first module load.
  const protocolPattern = /^[a-z0-9.+-]+:/i;
  const portPattern = /:[0-9]*$/;
  const hostPattern = /^\/\/[^@/]+@[^@/]+/;

  // Special case for a simple path URL
  const simplePathPattern = /^(\/\/?(?!\/)[^?\s]*)(\?[^\s]*)?$/;

  const hostnameMaxLen = 255;

  function isIpv6Hostname(hostname) {
    return (
      StringPrototypeCharCodeAt(hostname, 0) === CHAR_LEFT_SQUARE_BRACKET &&
      StringPrototypeCharCodeAt(hostname, hostname.length - 1) === CHAR_RIGHT_SQUARE_BRACKET
    );
  }

  // This prevents some common spoofing bugs due to our use of IDNA toASCII. For
  // compatibility, the set of characters we use here is the *intersection* of
  // "forbidden host code point" in the WHATWG URL Standard [1] and the
  // characters in the host parsing loop in Url.prototype.parse, with the
  // following additions:
  //
  // - ':' since this could cause a "protocol spoofing" bug
  // - '@' since this could cause parts of the hostname to be confused with auth
  // - '[' and ']' since this could cause a non-IPv6 hostname to be interpreted
  //   as IPv6 by isIpv6Hostname above
  //
  // [1]: https://url.spec.whatwg.org/#forbidden-host-code-point
  const forbiddenHostChars = /[\0\t\n\r #%/:<>?@[\\\]^|]/;
  // For IPv6, permit '[', ']', and ':'.
  const forbiddenHostCharsIpv6 = /[\0\t\n\r #%/<>?@\\^|]/;

  Url.prototype.parse = function parse(url, parseQueryString, slashesDenoteHost) {
    validateString(url, 'url');

    // Copy chrome, IE, opera backslash-handling behavior.
    // Back slashes before the query string get converted to forward slashes
    // See: https://code.google.com/p/chromium/issues/detail?id=25916
    let hasHash = false;
    let hasAt = false;
    let start = -1;
    let end = -1;
    let rest = '';
    let lastPos = 0;
    for (let i = 0, inWs = false, split = false; i < url.length; ++i) {
      const code = url.charCodeAt(i);

      // Find first and last non-whitespace characters for trimming
      const isWs =
        code < 33 || code === CHAR_NO_BREAK_SPACE || code === CHAR_ZERO_WIDTH_NOBREAK_SPACE;
      if (start === -1) {
        if (isWs) {
          continue;
        }
        lastPos = start = i;
      } else if (inWs) {
        if (!isWs) {
          end = -1;
          inWs = false;
        }
      } else if (isWs) {
        end = i;
        inWs = true;
      }

      // Only convert backslashes while we haven't seen a split character
      if (!split) {
        switch (code) {
          case CHAR_AT:
            hasAt = true;
            break;
          case CHAR_HASH:
            hasHash = true;
          // Fall through
          case CHAR_QUESTION_MARK:
            split = true;
            break;
          case CHAR_BACKWARD_SLASH:
            if (i - lastPos > 0) {
              rest += url.slice(lastPos, i);
            }
            rest += '/';
            lastPos = i + 1;
            break;
        }
      } else if (!hasHash && code === CHAR_HASH) {
        hasHash = true;
      }
    }

    // Check if string was non-empty (including strings with only whitespace)
    if (start !== -1) {
      if (lastPos === start) {
        // We didn't convert any backslashes

        if (end === -1) {
          if (start === 0) {
            rest = url;
          } else {
            rest = url.slice(start);
          }
        } else {
          rest = url.slice(start, end);
        }
      } else if (end === -1 && lastPos < url.length) {
        // We converted some backslashes and have only part of the entire string
        rest += url.slice(lastPos);
      } else if (end !== -1 && lastPos < end) {
        // We converted some backslashes and have only part of the entire string
        rest += url.slice(lastPos, end);
      }
    }

    if (!slashesDenoteHost && !hasHash && !hasAt) {
      // Try fast path regexp
      const simplePath = simplePathPattern.exec(rest);
      if (simplePath) {
        this.path = rest;
        this.href = rest;
        this.pathname = simplePath[1];
        if (simplePath[2]) {
          this.search = simplePath[2];
          if (parseQueryString) {
            this.query = querystring.parse(this.search.slice(1));
          } else {
            this.query = this.search.slice(1);
          }
        } else if (parseQueryString) {
          this.search = null;
          this.query = { __proto__: null };
        }
        return this;
      }
    }

    let proto = protocolPattern.exec(rest);
    let lowerProto;
    if (proto) {
      proto = proto[0];
      lowerProto = proto.toLowerCase();
      this.protocol = lowerProto;
      rest = rest.slice(proto.length);
    }

    // Figure out if it's got a host
    // user@server is *always* interpreted as a hostname, and url
    // resolution will treat //foo/bar as host=foo,path=bar because that's
    // how the browser resolves relative URLs.
    let slashes;
    if (slashesDenoteHost || proto || hostPattern.test(rest)) {
      slashes =
        rest.charCodeAt(0) === CHAR_FORWARD_SLASH && rest.charCodeAt(1) === CHAR_FORWARD_SLASH;
      if (slashes && !(proto && hostlessProtocol.has(lowerProto))) {
        rest = rest.slice(2);
        this.slashes = true;
      }
    }

    if (!hostlessProtocol.has(lowerProto) && (slashes || (proto && !slashedProtocol.has(proto)))) {
      // there's a hostname.
      // the first instance of /, ?, ;, or # ends the host.
      //
      // If there is an @ in the hostname, then non-host chars *are* allowed
      // to the left of the last @ sign, unless some host-ending character
      // comes *before* the @-sign.
      // URLs are obnoxious.
      //
      // ex:
      // http://a@b@c/ => user:a@b host:c
      // http://a@b?@c => user:a host:b path:/?@c

      let hostEnd = -1;
      let atSign = -1;
      let nonHost = -1;
      for (let i = 0; i < rest.length; ++i) {
        switch (rest.charCodeAt(i)) {
          case CHAR_TAB:
          case CHAR_LINE_FEED:
          case CHAR_CARRIAGE_RETURN:
            // WHATWG URL removes tabs, newlines, and carriage returns. Let's do that too.
            rest = rest.slice(0, i) + rest.slice(i + 1);
            i -= 1;
            break;
          case CHAR_SPACE:
          case CHAR_DOUBLE_QUOTE:
          case CHAR_PERCENT:
          case CHAR_SINGLE_QUOTE:
          case CHAR_SEMICOLON:
          case CHAR_LEFT_ANGLE_BRACKET:
          case CHAR_RIGHT_ANGLE_BRACKET:
          case CHAR_BACKWARD_SLASH:
          case CHAR_CIRCUMFLEX_ACCENT:
          case CHAR_GRAVE_ACCENT:
          case CHAR_LEFT_CURLY_BRACKET:
          case CHAR_VERTICAL_LINE:
          case CHAR_RIGHT_CURLY_BRACKET:
            // Characters that are never ever allowed in a hostname from RFC 2396
            if (nonHost === -1) {
              nonHost = i;
            }
            break;
          case CHAR_HASH:
          case CHAR_FORWARD_SLASH:
          case CHAR_QUESTION_MARK:
            // Find the first instance of any host-ending characters
            if (nonHost === -1) {
              nonHost = i;
            }
            hostEnd = i;
            break;
          case CHAR_AT:
            // At this point, either we have an explicit point where the
            // auth portion cannot go past, or the last @ char is the decider.
            atSign = i;
            nonHost = -1;
            break;
        }
        if (hostEnd !== -1) {
          break;
        }
      }
      start = 0;
      if (atSign !== -1) {
        this.auth = decodeURIComponent(rest.slice(0, atSign));
        start = atSign + 1;
      }
      if (nonHost === -1) {
        this.host = rest.slice(start);
        rest = '';
      } else {
        this.host = rest.slice(start, nonHost);
        rest = rest.slice(nonHost);
      }

      // pull out port.
      this.parseHost();

      // We've indicated that there is a hostname,
      // so even if it's empty, it has to be present.
      if (typeof this.hostname !== 'string') {
        this.hostname = '';
      }

      const hostname = this.hostname;

      // If hostname begins with [ and ends with ]
      // assume that it's an IPv6 address.
      const ipv6Hostname = isIpv6Hostname(hostname);

      // validate a little.
      if (!ipv6Hostname) {
        rest = getHostname(this, rest, hostname, url);
      }

      if (this.hostname.length > hostnameMaxLen) {
        this.hostname = '';
      } else {
        // Hostnames are always lower case.
        this.hostname = this.hostname.toLowerCase();
      }

      if (this.hostname !== '') {
        if (ipv6Hostname) {
          if (forbiddenHostCharsIpv6.test(this.hostname)) {
            throw new ERR_INVALID_URL(url);
          }
        } else {
          // IDNA Support: Returns a punycoded representation of "domain".
          // It only converts parts of the domain name that
          // have non-ASCII characters, i.e. it doesn't matter if
          // you call it with a domain that already is ASCII-only.
          this.hostname = toASCII(this.hostname);

          // Prevent two potential routes of hostname spoofing.
          // 1. If this.hostname is empty, it must have become empty due to toASCII
          //    since we checked this.hostname above.
          // 2. If any of forbiddenHostChars appears in this.hostname, it must have
          //    also gotten in due to toASCII. This is since getHostname would have
          //    filtered them out otherwise.
          // Rather than trying to correct this by moving the non-host part into
          // the pathname as we've done in getHostname, throw an exception to
          // convey the severity of this issue.
          if (this.hostname === '' || forbiddenHostChars.test(this.hostname)) {
            throw new ERR_INVALID_URL(url);
          }
        }
      }

      const p = this.port ? ':' + this.port : '';
      const h = this.hostname || '';
      this.host = h + p;

      // strip [ and ] from the hostname
      // the host field still retains them, though
      if (ipv6Hostname) {
        this.hostname = this.hostname.slice(1, -1);
        if (rest[0] !== '/') {
          rest = '/' + rest;
        }
      }
    }

    // Now rest is set to the post-host stuff.
    // Chop off any delim chars.
    if (!unsafeProtocol.has(lowerProto)) {
      // First, make 100% sure that any "autoEscape" chars get
      // escaped, even if encodeURIComponent doesn't think they
      // need to be.
      rest = autoEscapeStr(rest);
    }

    let questionIdx = -1;
    let hashIdx = -1;
    for (let i = 0; i < rest.length; ++i) {
      const code = rest.charCodeAt(i);
      if (code === CHAR_HASH) {
        this.hash = rest.slice(i);
        hashIdx = i;
        break;
      } else if (code === CHAR_QUESTION_MARK && questionIdx === -1) {
        questionIdx = i;
      }
    }

    if (questionIdx !== -1) {
      if (hashIdx === -1) {
        this.search = rest.slice(questionIdx);
        this.query = rest.slice(questionIdx + 1);
      } else {
        this.search = rest.slice(questionIdx, hashIdx);
        this.query = rest.slice(questionIdx + 1, hashIdx);
      }
      if (parseQueryString) {
        this.query = querystring.parse(this.query);
      }
    } else if (parseQueryString) {
      // No query string, but parseQueryString still requested
      this.search = null;
      this.query = { __proto__: null };
    }

    const useQuestionIdx = questionIdx !== -1 && (hashIdx === -1 || questionIdx < hashIdx);
    const firstIdx = useQuestionIdx ? questionIdx : hashIdx;
    if (firstIdx === -1) {
      if (rest.length > 0) {
        this.pathname = rest;
      }
    } else if (firstIdx > 0) {
      this.pathname = rest.slice(0, firstIdx);
    }
    if (slashedProtocol.has(lowerProto) && this.hostname && !this.pathname) {
      this.pathname = '/';
    }

    // To support http.request
    if (this.pathname || this.search) {
      const p = this.pathname || '';
      const s = this.search || '';
      this.path = p + s;
    }

    // Finally, reconstruct the href based on what has been validated.
    this.href = this.format();
    return this;
  };

  function getHostname(self, rest, hostname, url) {
    for (let i = 0; i < hostname.length; ++i) {
      const code = hostname.charCodeAt(i);
      const isValid =
        code !== CHAR_FORWARD_SLASH &&
        code !== CHAR_BACKWARD_SLASH &&
        code !== CHAR_HASH &&
        code !== CHAR_QUESTION_MARK &&
        code !== CHAR_COLON;

      if (!isValid) {
        // If leftover starts with :, then it represents an invalid port.
        // But url.parse() is lenient about it for now.
        // Issue a warning and continue.
        self.hostname = hostname.slice(0, i);
        return `/${hostname.slice(i)}${rest}`;
      }
    }
    return rest;
  }

  // Escaped characters. Use empty strings to fill up unused entries.
  // Using Array is faster than Object/Map
  const escapedCodes = [
    /* 0 - 9 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '%09',
    /* 10 - 19 */ '%0A',
    '',
    '',
    '%0D',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 20 - 29 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 30 - 39 */ '',
    '',
    '%20',
    '',
    '%22',
    '',
    '',
    '',
    '',
    '%27',
    /* 40 - 49 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 50 - 59 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 60 - 69 */ '%3C',
    '',
    '%3E',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 70 - 79 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 80 - 89 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 90 - 99 */ '',
    '',
    '%5C',
    '',
    '%5E',
    '',
    '%60',
    '',
    '',
    '',
    /* 100 - 109 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 110 - 119 */ '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    '',
    /* 120 - 125 */ '',
    '',
    '',
    '%7B',
    '%7C',
    '%7D'
  ];

  // Automatically escape all delimiters and unwise characters from RFC 2396.
  // Also escape single quotes in case of an XSS attack.
  // Return the escaped string.
  function autoEscapeStr(rest) {
    let escaped = '';
    let lastEscapedPos = 0;
    for (let i = 0; i < rest.length; ++i) {
      // `escaped` contains substring up to the last escaped character.
      const escapedChar = escapedCodes[rest.charCodeAt(i)];
      if (escapedChar) {
        // Concat if there are ordinary characters in the middle.
        if (i > lastEscapedPos) {
          escaped += rest.slice(lastEscapedPos, i);
        }
        escaped += escapedChar;
        lastEscapedPos = i + 1;
      }
    }
    if (lastEscapedPos === 0) {
      // Nothing has been escaped.
      return rest;
    }

    // There are ordinary characters at the end.
    if (lastEscapedPos < rest.length) {
      escaped += rest.slice(lastEscapedPos);
    }

    return escaped;
  }

  Url.prototype.format = function format() {
    let auth = this.auth || '';
    if (auth) {
      auth = encodeAuth(auth);
      auth += '@';
    }

    let protocol = this.protocol || '';
    if (protocol && StringPrototypeCharCodeAt(protocol, protocol.length - 1) !== 58 /* : */) {
      protocol += ':';
    }

    let pathname = this.pathname || '';
    let hash = this.hash || '';
    let host = '';
    let query = '';

    if (this.host) {
      host = auth + this.host;
    } else if (this.hostname) {
      host =
        auth +
        (StringPrototypeIndexOf(this.hostname, ':') !== -1 && !isIpv6Hostname(this.hostname)
          ? '[' + this.hostname + ']'
          : this.hostname);
      if (this.port) {
        host += ':' + this.port;
      }
    }

    if (this.query !== null && typeof this.query === 'object') {
      query = querystring.stringify(this.query);
    }
    let search = this.search || (query && '?' + query) || '';

    if (
      StringPrototypeIndexOf(pathname, '#') !== -1 ||
      StringPrototypeIndexOf(pathname, '?') !== -1
    ) {
      let newPathname = '';
      let lastPos = 0;
      const len = pathname.length;
      for (let i = 0; i < len; i++) {
        const code = StringPrototypeCharCodeAt(pathname, i);
        if (code === CHAR_HASH || code === CHAR_QUESTION_MARK) {
          if (i > lastPos) {
            newPathname += StringPrototypeSlice(pathname, lastPos, i);
          }
          newPathname += code === CHAR_HASH ? '%23' : '%3F';
          lastPos = i + 1;
        }
      }
      if (lastPos < len) {
        newPathname += StringPrototypeSlice(pathname, lastPos);
      }
      pathname = newPathname;
    }

    // Only the slashedProtocols get the //.  Not mailto:, xmpp:, etc.
    // unless they had them to begin with.
    if (this.slashes || slashedProtocol.has(protocol)) {
      if (this.slashes || host) {
        if (pathname && StringPrototypeCharCodeAt(pathname, 0) !== CHAR_FORWARD_SLASH) {
          pathname = '/' + pathname;
        }
        host = '//' + host;
      } else if (
        protocol.length >= 4 &&
        StringPrototypeCharCodeAt(protocol, 0) === 102 /* f */ &&
        StringPrototypeCharCodeAt(protocol, 1) === 105 /* i */ &&
        StringPrototypeCharCodeAt(protocol, 2) === 108 /* l */ &&
        StringPrototypeCharCodeAt(protocol, 3) === 101 /* e */
      ) {
        host = '//';
      }
    }

    // Escape '#' in search.
    if (StringPrototypeIndexOf(search, '#') !== -1) {
      search = StringPrototypeReplaceAll(search, '#', '%23');
    }

    if (hash && StringPrototypeCharCodeAt(hash, 0) !== CHAR_HASH) {
      hash = '#' + hash;
    }
    if (search && StringPrototypeCharCodeAt(search, 0) !== CHAR_QUESTION_MARK) {
      search = '?' + search;
    }

    return protocol + host + pathname + search + hash;
  };

  Url.prototype.parseHost = function parseHost() {
    let host = this.host;
    let port = portPattern.exec(host);
    if (port) {
      port = port[0];
      if (port !== ':') {
        this.port = port.slice(1);
      }
      host = host.slice(0, host.length - port.length);
    }
    if (host) {
      this.hostname = host;
    }
  };

  function urlParse(value, parseQueryString, slashesDenoteHost) {
    if (value instanceof Url) {
      return value;
    }
    return new Url().parse(value, parseQueryString, slashesDenoteHost);
  }

  function unsupportedResolution() {
    throw Object.assign(new Error('Legacy URL resolution is not implemented'), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  }

  Url.prototype.resolve = unsupportedResolution;
  Url.prototype.resolveObject = unsupportedResolution;
  return { parse: urlParse, Url };
}
