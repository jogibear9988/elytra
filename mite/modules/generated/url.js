/* Generated from mite/modules/core/url.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/legacy-url.mjs
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
function createLegacyUrl(native) {
  const StringPrototypeCharCodeAt = (value, index) => value.charCodeAt(index);
  const StringPrototypeIndexOf = (value, search) => value.indexOf(search);
  const StringPrototypeReplaceAll = (value, search, replacement) => value.replaceAll(search, replacement);
  const StringPrototypeSlice = (value, start, end) => value.slice(start, end);
  const toASCII = (value) => native.asciiDomain(value);
  const unsafeProtocol = /* @__PURE__ */ new Set(["javascript", "javascript:"]);
  const hostlessProtocol = /* @__PURE__ */ new Set(["javascript", "javascript:"]);
  const slashedProtocol = new Set(
    ["http", "https", "ftp", "gopher", "file", "ws", "wss"].flatMap((value) => [value, value + ":"])
  );
  class ERR_INVALID_URL extends TypeError {
    constructor(input) {
      super("Invalid URL");
      this.code = "ERR_INVALID_URL";
      this.input = input;
    }
  }
  function validateString(value) {
    if (typeof value !== "string") {
      throw Object.assign(new TypeError("url must be a string"), { code: "ERR_INVALID_ARG_TYPE" });
    }
  }
  const hexTable = Array.from(
    { length: 256 },
    (_, value) => "%" + value.toString(16).toUpperCase().padStart(2, "0")
  );
  const noEscapeQuery = Array.from(
    { length: 128 },
    (_, value) => /[A-Za-z0-9!\-._~'()*]/.test(String.fromCharCode(value)) ? 1 : 0
  );
  const noEscapeAuth = noEscapeQuery.slice();
  noEscapeAuth[58] = 1;
  class ERR_INVALID_URI extends URIError {
    constructor() {
      super("URI malformed");
      this.code = "ERR_INVALID_URI";
    }
  }
  function encodeStr(str, noEscapeTable, hexTable2) {
    const len = str.length;
    if (len === 0) {
      return "";
    }
    let out = "";
    let lastPos = 0;
    let i = 0;
    outer: for (; i < len; i++) {
      let c = StringPrototypeCharCodeAt(str, i);
      while (c < 128) {
        if (noEscapeTable[c] !== 1) {
          if (lastPos < i) {
            out += StringPrototypeSlice(str, lastPos, i);
          }
          lastPos = i + 1;
          out += hexTable2[c];
        }
        if (++i === len) {
          break outer;
        }
        c = StringPrototypeCharCodeAt(str, i);
      }
      if (lastPos < i) {
        out += StringPrototypeSlice(str, lastPos, i);
      }
      if (c < 2048) {
        lastPos = i + 1;
        out += hexTable2[192 | c >> 6] + hexTable2[128 | c & 63];
        continue;
      }
      if (c < 55296 || c >= 57344) {
        lastPos = i + 1;
        out += hexTable2[224 | c >> 12] + hexTable2[128 | c >> 6 & 63] + hexTable2[128 | c & 63];
        continue;
      }
      ++i;
      if (i >= len) {
        throw new ERR_INVALID_URI();
      }
      const c2 = StringPrototypeCharCodeAt(str, i) & 1023;
      lastPos = i + 1;
      c = 65536 + ((c & 1023) << 10 | c2);
      out += hexTable2[240 | c >> 18] + hexTable2[128 | c >> 12 & 63] + hexTable2[128 | c >> 6 & 63] + hexTable2[128 | c & 63];
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
    value = value.replaceAll("+", " ");
    if (!/%[0-9a-f]{2}/i.test(value)) {
      return value;
    }
    try {
      return decodeURIComponent(value);
    } catch {
      const bytes = [];
      for (let index = 0; index < value.length; ++index) {
        const pair = value.slice(index + 1, index + 3);
        if (value[index] === "%" && /^[0-9a-f]{2}$/i.test(pair)) {
          bytes.push(hexTable[parseInt(pair, 16)]);
          index += 2;
        } else {
          bytes.push(hexTable[value.charCodeAt(index) & 255]);
        }
      }
      return native.decodeForm("value=" + bytes.join(""))[0]?.[1] ?? "";
    }
  }
  const querystring = {
    parse(value) {
      const result = /* @__PURE__ */ Object.create(null);
      let end = -1;
      for (let count = 0; count < 1e3; ++count) {
        end = value.indexOf("&", end + 1);
        if (end === -1) {
          break;
        }
      }
      const selected = end === -1 ? value : value.slice(0, end);
      for (const pair of selected.split("&")) {
        if (!pair) {
          continue;
        }
        const equal = pair.indexOf("=");
        const name = decodeQuery(equal < 0 ? pair : pair.slice(0, equal));
        const item = decodeQuery(equal < 0 ? "" : pair.slice(equal + 1));
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
      const scalar = (item) => typeof item === "string" || typeof item === "boolean" || typeof item === "number" && Number.isFinite(item) || typeof item === "bigint" ? String(item) : "";
      for (const name of Object.keys(value)) {
        const items = Array.isArray(value[name]) ? value[name] : [value[name]];
        for (const item of items) {
          pairs.push(
            encodeStr(name, noEscapeQuery, hexTable) + "=" + encodeStr(scalar(item), noEscapeQuery, hexTable)
          );
        }
      }
      return pairs.join("&");
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
  const protocolPattern = /^[a-z0-9.+-]+:/i;
  const portPattern = /:[0-9]*$/;
  const hostPattern = /^\/\/[^@/]+@[^@/]+/;
  const simplePathPattern = /^(\/\/?(?!\/)[^?\s]*)(\?[^\s]*)?$/;
  const hostnameMaxLen = 255;
  function isIpv6Hostname(hostname) {
    return StringPrototypeCharCodeAt(hostname, 0) === CHAR_LEFT_SQUARE_BRACKET && StringPrototypeCharCodeAt(hostname, hostname.length - 1) === CHAR_RIGHT_SQUARE_BRACKET;
  }
  const forbiddenHostChars = /[\0\t\n\r #%/:<>?@[\\\]^|]/;
  const forbiddenHostCharsIpv6 = /[\0\t\n\r #%/<>?@\\^|]/;
  Url.prototype.parse = function parse(url, parseQueryString, slashesDenoteHost) {
    validateString(url, "url");
    let hasHash = false;
    let hasAt = false;
    let start = -1;
    let end = -1;
    let rest = "";
    let lastPos = 0;
    for (let i = 0, inWs = false, split = false; i < url.length; ++i) {
      const code = url.charCodeAt(i);
      const isWs = code < 33 || code === CHAR_NO_BREAK_SPACE || code === CHAR_ZERO_WIDTH_NOBREAK_SPACE;
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
            rest += "/";
            lastPos = i + 1;
            break;
        }
      } else if (!hasHash && code === CHAR_HASH) {
        hasHash = true;
      }
    }
    if (start !== -1) {
      if (lastPos === start) {
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
        rest += url.slice(lastPos);
      } else if (end !== -1 && lastPos < end) {
        rest += url.slice(lastPos, end);
      }
    }
    if (!slashesDenoteHost && !hasHash && !hasAt) {
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
    let slashes;
    if (slashesDenoteHost || proto || hostPattern.test(rest)) {
      slashes = rest.charCodeAt(0) === CHAR_FORWARD_SLASH && rest.charCodeAt(1) === CHAR_FORWARD_SLASH;
      if (slashes && !(proto && hostlessProtocol.has(lowerProto))) {
        rest = rest.slice(2);
        this.slashes = true;
      }
    }
    if (!hostlessProtocol.has(lowerProto) && (slashes || proto && !slashedProtocol.has(proto))) {
      let hostEnd = -1;
      let atSign = -1;
      let nonHost = -1;
      for (let i = 0; i < rest.length; ++i) {
        switch (rest.charCodeAt(i)) {
          case CHAR_TAB:
          case CHAR_LINE_FEED:
          case CHAR_CARRIAGE_RETURN:
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
            if (nonHost === -1) {
              nonHost = i;
            }
            break;
          case CHAR_HASH:
          case CHAR_FORWARD_SLASH:
          case CHAR_QUESTION_MARK:
            if (nonHost === -1) {
              nonHost = i;
            }
            hostEnd = i;
            break;
          case CHAR_AT:
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
        rest = "";
      } else {
        this.host = rest.slice(start, nonHost);
        rest = rest.slice(nonHost);
      }
      this.parseHost();
      if (typeof this.hostname !== "string") {
        this.hostname = "";
      }
      const hostname = this.hostname;
      const ipv6Hostname = isIpv6Hostname(hostname);
      if (!ipv6Hostname) {
        rest = getHostname(this, rest, hostname, url);
      }
      if (this.hostname.length > hostnameMaxLen) {
        this.hostname = "";
      } else {
        this.hostname = this.hostname.toLowerCase();
      }
      if (this.hostname !== "") {
        if (ipv6Hostname) {
          if (forbiddenHostCharsIpv6.test(this.hostname)) {
            throw new ERR_INVALID_URL(url);
          }
        } else {
          this.hostname = toASCII(this.hostname);
          if (this.hostname === "" || forbiddenHostChars.test(this.hostname)) {
            throw new ERR_INVALID_URL(url);
          }
        }
      }
      const p = this.port ? ":" + this.port : "";
      const h = this.hostname || "";
      this.host = h + p;
      if (ipv6Hostname) {
        this.hostname = this.hostname.slice(1, -1);
        if (rest[0] !== "/") {
          rest = "/" + rest;
        }
      }
    }
    if (!unsafeProtocol.has(lowerProto)) {
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
      this.pathname = "/";
    }
    if (this.pathname || this.search) {
      const p = this.pathname || "";
      const s = this.search || "";
      this.path = p + s;
    }
    this.href = this.format();
    return this;
  };
  function getHostname(self, rest, hostname, url) {
    for (let i = 0; i < hostname.length; ++i) {
      const code = hostname.charCodeAt(i);
      const isValid = code !== CHAR_FORWARD_SLASH && code !== CHAR_BACKWARD_SLASH && code !== CHAR_HASH && code !== CHAR_QUESTION_MARK && code !== CHAR_COLON;
      if (!isValid) {
        self.hostname = hostname.slice(0, i);
        return `/${hostname.slice(i)}${rest}`;
      }
    }
    return rest;
  }
  const escapedCodes = [
    /* 0 - 9 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "%09",
    /* 10 - 19 */
    "%0A",
    "",
    "",
    "%0D",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 20 - 29 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 30 - 39 */
    "",
    "",
    "%20",
    "",
    "%22",
    "",
    "",
    "",
    "",
    "%27",
    /* 40 - 49 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 50 - 59 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 60 - 69 */
    "%3C",
    "",
    "%3E",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 70 - 79 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 80 - 89 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 90 - 99 */
    "",
    "",
    "%5C",
    "",
    "%5E",
    "",
    "%60",
    "",
    "",
    "",
    /* 100 - 109 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 110 - 119 */
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    /* 120 - 125 */
    "",
    "",
    "",
    "%7B",
    "%7C",
    "%7D"
  ];
  function autoEscapeStr(rest) {
    let escaped = "";
    let lastEscapedPos = 0;
    for (let i = 0; i < rest.length; ++i) {
      const escapedChar = escapedCodes[rest.charCodeAt(i)];
      if (escapedChar) {
        if (i > lastEscapedPos) {
          escaped += rest.slice(lastEscapedPos, i);
        }
        escaped += escapedChar;
        lastEscapedPos = i + 1;
      }
    }
    if (lastEscapedPos === 0) {
      return rest;
    }
    if (lastEscapedPos < rest.length) {
      escaped += rest.slice(lastEscapedPos);
    }
    return escaped;
  }
  Url.prototype.format = function format() {
    let auth = this.auth || "";
    if (auth) {
      auth = encodeAuth(auth);
      auth += "@";
    }
    let protocol = this.protocol || "";
    if (protocol && StringPrototypeCharCodeAt(protocol, protocol.length - 1) !== 58) {
      protocol += ":";
    }
    let pathname = this.pathname || "";
    let hash = this.hash || "";
    let host = "";
    let query = "";
    if (this.host) {
      host = auth + this.host;
    } else if (this.hostname) {
      host = auth + (StringPrototypeIndexOf(this.hostname, ":") !== -1 && !isIpv6Hostname(this.hostname) ? "[" + this.hostname + "]" : this.hostname);
      if (this.port) {
        host += ":" + this.port;
      }
    }
    if (this.query !== null && typeof this.query === "object") {
      query = querystring.stringify(this.query);
    }
    let search = this.search || query && "?" + query || "";
    if (StringPrototypeIndexOf(pathname, "#") !== -1 || StringPrototypeIndexOf(pathname, "?") !== -1) {
      let newPathname = "";
      let lastPos = 0;
      const len = pathname.length;
      for (let i = 0; i < len; i++) {
        const code = StringPrototypeCharCodeAt(pathname, i);
        if (code === CHAR_HASH || code === CHAR_QUESTION_MARK) {
          if (i > lastPos) {
            newPathname += StringPrototypeSlice(pathname, lastPos, i);
          }
          newPathname += code === CHAR_HASH ? "%23" : "%3F";
          lastPos = i + 1;
        }
      }
      if (lastPos < len) {
        newPathname += StringPrototypeSlice(pathname, lastPos);
      }
      pathname = newPathname;
    }
    if (this.slashes || slashedProtocol.has(protocol)) {
      if (this.slashes || host) {
        if (pathname && StringPrototypeCharCodeAt(pathname, 0) !== CHAR_FORWARD_SLASH) {
          pathname = "/" + pathname;
        }
        host = "//" + host;
      } else if (protocol.length >= 4 && StringPrototypeCharCodeAt(protocol, 0) === 102 && StringPrototypeCharCodeAt(protocol, 1) === 105 && StringPrototypeCharCodeAt(protocol, 2) === 108 && StringPrototypeCharCodeAt(protocol, 3) === 101) {
        host = "//";
      }
    }
    if (StringPrototypeIndexOf(search, "#") !== -1) {
      search = StringPrototypeReplaceAll(search, "#", "%23");
    }
    if (hash && StringPrototypeCharCodeAt(hash, 0) !== CHAR_HASH) {
      hash = "#" + hash;
    }
    if (search && StringPrototypeCharCodeAt(search, 0) !== CHAR_QUESTION_MARK) {
      search = "?" + search;
    }
    return protocol + host + pathname + search + hash;
  };
  Url.prototype.parseHost = function parseHost() {
    let host = this.host;
    let port = portPattern.exec(host);
    if (port) {
      port = port[0];
      if (port !== ":") {
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
    throw Object.assign(new Error("Legacy URL resolution is not implemented"), {
      code: "ERR_MITE_UNSUPPORTED_API"
    });
  }
  Url.prototype.resolve = unsupportedResolution;
  Url.prototype.resolveObject = unsupportedResolution;
  return { parse: urlParse, Url };
}

// mite/modules/core/url.mjs
function createUrlModule(host) {
  const native = host.native;
  const urls = /* @__PURE__ */ new WeakMap();
  const params = /* @__PURE__ */ new WeakMap();
  function fail(code, message) {
    const error = new TypeError(message);
    error.code = code;
    throw error;
  }
  function usv(value) {
    return `${value}`.toWellFormed();
  }
  function state(map, object) {
    const value = map.get(object);
    if (!value) {
      if (map === urls) {
        throw new TypeError("Invalid URL receiver");
      }
      fail("ERR_INVALID_THIS", "Invalid receiver");
    }
    return value;
  }
  function required(count, actual) {
    if (actual < count) {
      fail("ERR_MISSING_ARGS", "Missing required argument");
    }
  }
  function invalid(input, base) {
    const error = new TypeError("Invalid URL");
    error.code = "ERR_INVALID_URL";
    error.input = input;
    if (base !== void 0) {
      error.base = base;
    }
    throw error;
  }
  function updateParams(value) {
    if (value.url) {
      value.url.record = native.update(
        value.url.record.href,
        "search",
        native.encodeForm(value.list)
      );
    }
  }
  class URLSearchParams {
    constructor(init = "") {
      const value = { list: [], url: null };
      params.set(this, value);
      if (init == null) {
        return;
      }
      if (typeof init === "object" || typeof init === "function") {
        const iterator2 = init[Symbol.iterator];
        if (iterator2 !== void 0 && iterator2 !== null) {
          if (typeof iterator2 !== "function") {
            fail("ERR_ARG_NOT_ITERABLE", "Query pairs must be iterable");
          }
          for (const pair of init) {
            if (pair === null || typeof pair !== "object" && typeof pair !== "function" || typeof pair[Symbol.iterator] !== "function") {
              fail("ERR_INVALID_TUPLE", "Each query pair must be iterable with two items");
            }
            const items = Array.from(pair);
            if (items.length !== 2) {
              fail("ERR_INVALID_TUPLE", "Each query pair must contain exactly two items");
            }
            value.list.push([usv(items[0]), usv(items[1])]);
          }
        } else {
          const names = /* @__PURE__ */ new Map();
          for (const name of Reflect.ownKeys(init)) {
            if (!Object.getOwnPropertyDescriptor(init, name).enumerable) {
              continue;
            }
            const key = usv(name);
            const item = usv(init[name]);
            if (names.has(key)) {
              value.list[names.get(key)][1] = item;
            } else {
              names.set(key, value.list.length);
              value.list.push([key, item]);
            }
          }
        }
      } else {
        const text = usv(init);
        value.list = native.decodeForm(text.startsWith("?") ? text.slice(1) : text);
      }
    }
    get size() {
      return state(params, this).list.length;
    }
    append(name, value) {
      const s = state(params, this);
      required(2, arguments.length);
      s.list.push([usv(name), usv(value)]);
      updateParams(s);
    }
    delete(name, value) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      if (value !== void 0) {
        value = usv(value);
      }
      s.list = s.list.filter(
        (pair) => pair[0] !== name || value !== void 0 && pair[1] !== value
      );
      updateParams(s);
    }
    get(name) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      return s.list.find((pair) => pair[0] === name)?.[1] ?? null;
    }
    getAll(name) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      return s.list.filter((pair) => pair[0] === name).map((pair) => pair[1]);
    }
    has(name, value) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      if (value !== void 0) {
        value = usv(value);
      }
      return s.list.some((pair) => pair[0] === name && (value === void 0 || pair[1] === value));
    }
    set(name, value) {
      const s = state(params, this);
      required(2, arguments.length);
      name = usv(name);
      value = usv(value);
      let found = false;
      s.list = s.list.filter((pair) => {
        if (pair[0] !== name) {
          return true;
        }
        if (found) {
          return false;
        }
        pair[1] = value;
        found = true;
        return true;
      });
      if (!found) {
        s.list.push([name, value]);
      }
      updateParams(s);
    }
    sort() {
      const s = state(params, this);
      s.list.sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0);
      updateParams(s);
    }
    entries() {
      return iterator(state(params, this), "entries");
    }
    keys() {
      return iterator(state(params, this), "keys");
    }
    values() {
      return iterator(state(params, this), "values");
    }
    forEach(callback, thisArg) {
      const s = state(params, this);
      if (typeof callback !== "function") {
        fail("ERR_INVALID_ARG_TYPE", "callback must be a function");
      }
      for (let i = 0; i < s.list.length; ++i) {
        callback.call(thisArg, s.list[i][1], s.list[i][0], this);
      }
    }
    toString() {
      return native.encodeForm(state(params, this).list);
    }
  }
  const iterators = /* @__PURE__ */ new WeakMap();
  const iteratorPrototype = Object.create(
    Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()))
  );
  Object.defineProperty(iteratorPrototype, Symbol.toStringTag, {
    value: "URLSearchParams Iterator",
    configurable: true
  });
  Object.defineProperty(iteratorPrototype, "next", {
    enumerable: true,
    configurable: true,
    writable: true,
    value: function next() {
      const s = state(iterators, this);
      if (s.index >= s.source.list.length) {
        return { value: void 0, done: true };
      }
      const pair = s.source.list[s.index++];
      return {
        value: s.kind === "keys" ? pair[0] : s.kind === "values" ? pair[1] : pair.slice(),
        done: false
      };
    }
  });
  function iterator(source, kind) {
    const value = Object.create(iteratorPrototype);
    iterators.set(value, { source, kind, index: 0 });
    return value;
  }
  URLSearchParams.prototype[Symbol.iterator] = URLSearchParams.prototype.entries;
  Object.defineProperty(URLSearchParams.prototype, Symbol.toStringTag, {
    value: "URLSearchParams",
    configurable: true
  });
  class URL {
    constructor(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== void 0) {
        base = usv(base);
      }
      const record = native.parse(input, base);
      if (!record) {
        invalid(input, base);
      }
      const searchParams = new URLSearchParams(record.search);
      const value = { record, searchParams };
      urls.set(this, value);
      Object.assign(params.get(searchParams), { url: value, owner: this });
    }
    static canParse(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== void 0) {
        base = usv(base);
      }
      return native.parse(input, base) !== null;
    }
    static parse(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== void 0) {
        base = usv(base);
      }
      const result = native.parse(input, base);
      return result ? new URL(result.href) : null;
    }
    get searchParams() {
      return state(urls, this).searchParams;
    }
    get origin() {
      return state(urls, this).record.origin;
    }
    toString() {
      const s = urls.get(this);
      if (!s) {
        throw new TypeError("Invalid URL receiver");
      }
      return s.record.href;
    }
    toJSON() {
      return state(urls, this).record.href;
    }
  }
  for (const name of [
    "href",
    "protocol",
    "username",
    "password",
    "host",
    "hostname",
    "port",
    "pathname",
    "search",
    "hash"
  ]) {
    Object.defineProperty(URL.prototype, name, {
      enumerable: true,
      configurable: true,
      get() {
        return state(urls, this).record[name];
      },
      set(input) {
        const s = state(urls, this);
        input = usv(input);
        const value = name === "href" ? native.parse(input) : native.update(s.record.href, name, input);
        if (!value) {
          invalid(input);
        }
        s.record = value;
        if (name === "href" || name === "search") {
          params.get(s.searchParams).list = native.decodeForm(value.search.slice(1));
        }
      }
    });
  }
  Object.defineProperty(URL.prototype, Symbol.toStringTag, { value: "URL", configurable: true });
  for (const prototype of [URL.prototype, URLSearchParams.prototype]) {
    for (const name of Object.getOwnPropertyNames(prototype)) {
      if (name !== "constructor") {
        Object.defineProperty(prototype, name, {
          ...Object.getOwnPropertyDescriptor(prototype, name),
          enumerable: true
        });
      }
    }
  }
  const Buffer = host.Buffer;
  function fileURL(input) {
    if (typeof input === "string") {
      input = new URL(input);
    } else if (!urls.has(input)) {
      fail("ERR_INVALID_ARG_TYPE", "path must be a string or URL");
    }
    if (input.protocol !== "file:") {
      fail("ERR_INVALID_URL_SCHEME", "URL must use the file scheme");
    }
    return input;
  }
  function windowsOption(options) {
    return options?.windows ?? host.platform === "win32";
  }
  function validateFileURL(input, windows, raw = false) {
    const url = fileURL(input);
    if (!windows && url.hostname !== "") {
      fail("ERR_INVALID_FILE_URL_HOST", "File URL host must be localhost or empty");
    }
    if ((windows || !raw) && (windows ? /%2f|%5c/i : /%2f/i).test(url.pathname)) {
      invalidFilePath(url);
    }
    return url;
  }
  function invalidFilePath(url) {
    const error = new TypeError("Invalid file URL path");
    error.code = "ERR_INVALID_FILE_URL_PATH";
    error.input = url.href;
    throw error;
  }
  function fileURLToPath(input, options) {
    const windows = windowsOption(options);
    const url = validateFileURL(input, windows);
    let pathname = decodeURIComponent(url.pathname);
    if (!windows) {
      return pathname;
    }
    pathname = pathname.replaceAll("/", "\\");
    if (url.hostname) {
      return "\\\\" + native.unicodeHost(url.hostname) + pathname;
    }
    if (!/^\\[a-zA-Z]:/.test(pathname)) {
      invalidFilePath(url);
    }
    return pathname.slice(1);
  }
  function fileURLToPathBuffer(input, options) {
    const windows = windowsOption(options);
    const url = validateFileURL(input, windows, true);
    const raw = native.decodePath(url.pathname);
    const pathname = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
    if (!windows) {
      return pathname;
    }
    for (let i = 0; i < pathname.length; ++i) {
      if (pathname[i] === 47) {
        pathname[i] = 92;
      }
    }
    if (url.hostname) {
      return Buffer.concat([Buffer.from("\\\\" + native.unicodeHost(url.hostname)), pathname]);
    }
    if (pathname.length < 3 || pathname[0] !== 92 || !(pathname[1] >= 65 && pathname[1] <= 90 || pathname[1] >= 97 && pathname[1] <= 122) || pathname[2] !== 58) {
      invalidFilePath(url);
    }
    return pathname.subarray(1);
  }
  function pathToFileURL(path, options) {
    if (typeof path !== "string") {
      fail("ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    const windows = windowsOption(options);
    function encode(value) {
      return encodeURI(value.toWellFormed()).replace(
        /[?#~]/g,
        (c) => "%" + c.charCodeAt(0).toString(16).toUpperCase()
      );
    }
    function unc(value) {
      const slash = value.indexOf("/", 2);
      if (slash < 0) {
        fail("ERR_INVALID_ARG_VALUE", "UNC path must contain a resource path");
      }
      const hostname = native.asciiHost(value.slice(2, slash));
      if (!hostname) {
        fail("ERR_INVALID_ARG_VALUE", "Invalid UNC hostname");
      }
      return new URL("file://" + hostname + encode(value.slice(slash)));
    }
    if (windows && path.startsWith("\\\\?\\")) {
      path = path.slice(4);
      if (path.startsWith("UNC\\")) {
        return unc("//" + path.slice(4).replaceAll("\\", "/"));
      }
      return new URL("file:///" + encode(path.replaceAll("\\", "/")));
    }
    if (windows && path.startsWith("\\\\")) {
      return unc(path.replaceAll("\\", "/"));
    }
    let resolved = (windows ? host.path.win32 : host.path.posix).resolve(path);
    if (windows && resolved.startsWith("\\\\")) {
      return unc(resolved.replaceAll("\\", "/"));
    }
    const trailing = path.endsWith("/") || windows && path.endsWith("\\");
    if (trailing && !resolved.endsWith(host.path.sep)) {
      resolved += "/";
    }
    if (windows) {
      resolved = resolved.replaceAll("\\", "/");
    }
    return new URL("file://" + (resolved.startsWith("/") ? "" : "/") + encode(resolved));
  }
  function domainToASCII(domain) {
    required(1, arguments.length);
    domain = usv(domain);
    const authority = domain.replaceAll(String.fromCharCode(9), "").replaceAll(String.fromCharCode(10), "").replaceAll(String.fromCharCode(13), "").split(/[/?#\\]/, 1)[0];
    return native.asciiHost(authority) ?? "";
  }
  function domainToUnicode(domain) {
    required(1, arguments.length);
    const ascii = domainToASCII(domain);
    return ascii ? native.unicodeHost(ascii) : "";
  }
  function urlToHttpOptions(url) {
    if (url === null || typeof url !== "object") {
      fail("ERR_INVALID_ARG_TYPE", "url must be an object");
    }
    const options = {
      __proto__: null,
      ...url,
      protocol: url.protocol,
      hostname: url.hostname,
      hash: url.hash,
      search: url.search,
      pathname: url.pathname,
      path: (url.pathname || "") + (url.search || ""),
      href: url.href
    };
    if (options.hostname?.startsWith("[")) {
      options.hostname = options.hostname.slice(1, -1);
    }
    if (url.port !== "") {
      options.port = Number(url.port);
    }
    if (url.username || url.password) {
      options.auth = decodeURIComponent(url.username) + ":" + decodeURIComponent(url.password);
    }
    return options;
  }
  return {
    ...createLegacyUrl(native),
    URL,
    URLSearchParams,
    fileURLToPath,
    fileURLToPathBuffer,
    pathToFileURL,
    domainToASCII,
    domainToUnicode,
    urlToHttpOptions
  };
}
return createUrlModule;
})()
