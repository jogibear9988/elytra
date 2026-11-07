/**
 * HTTP token/header validation, duplicate-field policy and protocol metadata.
 * Build one instance per builtin factory so exported mutable tables retain their
 * existing realm ownership. Native parsing remains in the llhttp host adapter.
 */
export default function createHttpProtocol(native) {
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;
  const singleton = new Set([
    'age',
    'authorization',
    'content-length',
    'content-type',
    'etag',
    'expires',
    'from',
    'host',
    'if-modified-since',
    'if-unmodified-since',
    'last-modified',
    'location',
    'max-forwards',
    'proxy-authorization',
    'referer',
    'retry-after',
    'server',
    'user-agent'
  ]);
  const STATUS_CODES = {
    100: 'Continue',
    101: 'Switching Protocols',
    102: 'Processing',
    103: 'Early Hints',
    200: 'OK',
    201: 'Created',
    202: 'Accepted',
    203: 'Non-Authoritative Information',
    204: 'No Content',
    205: 'Reset Content',
    206: 'Partial Content',
    207: 'Multi-Status',
    208: 'Already Reported',
    226: 'IM Used',
    300: 'Multiple Choices',
    301: 'Moved Permanently',
    302: 'Found',
    303: 'See Other',
    304: 'Not Modified',
    305: 'Use Proxy',
    307: 'Temporary Redirect',
    308: 'Permanent Redirect',
    400: 'Bad Request',
    401: 'Unauthorized',
    402: 'Payment Required',
    403: 'Forbidden',
    404: 'Not Found',
    405: 'Method Not Allowed',
    406: 'Not Acceptable',
    407: 'Proxy Authentication Required',
    408: 'Request Timeout',
    409: 'Conflict',
    410: 'Gone',
    411: 'Length Required',
    412: 'Precondition Failed',
    413: 'Payload Too Large',
    414: 'URI Too Long',
    415: 'Unsupported Media Type',
    416: 'Range Not Satisfiable',
    417: 'Expectation Failed',
    418: "I'm a Teapot",
    421: 'Misdirected Request',
    422: 'Unprocessable Entity',
    423: 'Locked',
    424: 'Failed Dependency',
    425: 'Too Early',
    426: 'Upgrade Required',
    428: 'Precondition Required',
    429: 'Too Many Requests',
    431: 'Request Header Fields Too Large',
    451: 'Unavailable For Legal Reasons',
    500: 'Internal Server Error',
    501: 'Not Implemented',
    502: 'Bad Gateway',
    503: 'Service Unavailable',
    504: 'Gateway Timeout',
    505: 'HTTP Version Not Supported',
    506: 'Variant Also Negotiates',
    507: 'Insufficient Storage',
    508: 'Loop Detected',
    509: 'Bandwidth Limit Exceeded',
    510: 'Not Extended',
    511: 'Network Authentication Required'
  };
  const METHODS = [...native.METHODS].sort();

  function error(Type, code, message) {
    const e = new Type(message);
    e.code = code;
    return e;
  }

  function unsupported(name) {
    throw error(Error, 'ERR_MITE_UNSUPPORTED_API', `${name} is not implemented`);
  }

  function validateHeaderName(name) {
    if (typeof name !== 'string' || !token.test(name)) {
      throw error(
        TypeError,
        'ERR_INVALID_HTTP_TOKEN',
        `Header name must be a valid HTTP token [${String(name)}]`
      );
    }
  }

  function validateHeaderValue(name, value) {
    if (value === undefined) {
      throw error(TypeError, 'ERR_HTTP_INVALID_HEADER_VALUE', `Invalid value for header ${name}`);
    }
    const values = Array.isArray(value) ? value : [value];
    for (const item of values) {
      if (/[^\t\x20-\x7e\x80-\xff]/.test(String(item))) {
        throw error(TypeError, 'ERR_INVALID_CHAR', `Invalid character in header content [${name}]`);
      }
    }
  }

  function merged(raw, join) {
    const headers = {};
    const distinct = {};
    for (let i = 0; i < raw.length; i += 2) {
      const name = raw[i].toLowerCase();
      const value = raw[i + 1].replace(/^[ \t]+|[ \t]+$/g, '');
      if (!Object.hasOwn(distinct, name)) {
        Object.defineProperty(distinct, name, {
          value: [],
          enumerable: true,
          writable: true,
          configurable: true
        });
      }
      distinct[name].push(value);
      if (!Object.hasOwn(headers, name)) {
        Object.defineProperty(headers, name, {
          value: name === 'set-cookie' ? [value] : value,
          enumerable: true,
          writable: true,
          configurable: true
        });
      } else if (name === 'set-cookie') {
        headers[name].push(value);
      } else if (name === 'cookie') {
        headers[name] += '; ' + value;
      } else if (join || !singleton.has(name)) {
        headers[name] += ', ' + value;
      }
    }
    return { headers, distinct };
  }
  /** Revalidate mutable header values when serializing, including CR/LF and framing ambiguity. */
  function headerLines(headers) {
    let text = '';
    for (const { name, value } of headers.values()) {
      const values = Array.isArray(value)
        ? name.toLowerCase() === 'cookie'
          ? [value.join('; ')]
          : value
        : [value];
      for (const item of values) {
        const field = String(item);
        validateHeaderValue(name, field);
        text += `${name}: ${field}\r\n`;
      }
    }
    return text;
  }

  function pairs(raw) {
    if (raw.length % 2) {
      throw error(TypeError, 'ERR_INVALID_ARG_VALUE', 'Header array must have pairs');
    }
    const result = [];
    for (let i = 0; i < raw.length; i += 2) {
      result.push([raw[i], raw[i + 1]]);
    }
    return result;
  }

  function integer(value, name) {
    if (typeof value !== 'number') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < 0 || value > 2147483647) {
      throw error(RangeError, 'ERR_OUT_OF_RANGE', `Invalid ${name}`);
    }
    return value;
  }

  return {
    METHODS,
    STATUS_CODES,
    token,
    error,
    unsupported,
    validateHeaderName,
    validateHeaderValue,
    merged,
    headerLines,
    pairs,
    integer
  };
}
