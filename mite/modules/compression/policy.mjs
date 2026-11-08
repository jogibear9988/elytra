/** Explicit supported zlib options, bounds and errors; no native handles here. */
const constants = Object.freeze({
  Z_NO_FLUSH: 0,
  Z_PARTIAL_FLUSH: 1,
  Z_SYNC_FLUSH: 2,
  Z_FULL_FLUSH: 3,
  Z_FINISH: 4,
  Z_BLOCK: 5,
  Z_TREES: 6,
  Z_OK: 0,
  Z_STREAM_END: 1,
  Z_NEED_DICT: 2,
  Z_ERRNO: -1,
  Z_STREAM_ERROR: -2,
  Z_DATA_ERROR: -3,
  Z_MEM_ERROR: -4,
  Z_BUF_ERROR: -5,
  Z_VERSION_ERROR: -6,
  Z_NO_COMPRESSION: 0,
  Z_BEST_SPEED: 1,
  Z_BEST_COMPRESSION: 9,
  Z_DEFAULT_COMPRESSION: -1,
  Z_FILTERED: 1,
  Z_HUFFMAN_ONLY: 2,
  Z_RLE: 3,
  Z_FIXED: 4,
  Z_DEFAULT_STRATEGY: 0,
  Z_DEFLATED: 8
});

const error = (code, message, Type = Error) => Object.assign(new Type(message), { code });

const unsupported = (name) => {
  throw error('ERR_MITE_UNSUPPORTED_API', `node:zlib ${name} is not implemented`);
};

function integer(value, fallback, min, max, name) {
  if (value === undefined) {
    return fallback;
  }
  if (typeof value !== 'number') {
    throw error('ERR_INVALID_ARG_TYPE', `${name} must be a number`, TypeError);
  }
  if (!Number.isInteger(value) || value < min || value > max) {
    throw error('ERR_OUT_OF_RANGE', `${name} is out of range`, RangeError);
  }
  return value;
}

function flushKind(value, fallback) {
  const kind = integer(value, fallback, 0, 6, 'flush');
  if (![0, 2, 3, 4].includes(kind)) {
    unsupported(`flush kind ${kind}`);
  }
  return kind;
}

function options(value) {
  if (value === undefined || value === null) {
    value = {};
  }
  if (typeof value !== 'object') {
    throw error('ERR_INVALID_ARG_TYPE', 'options must be an object', TypeError);
  }
  for (const name of Object.keys(value)) {
    if (
      ![
        'windowBits',
        'level',
        'memLevel',
        'strategy',
        'chunkSize',
        'flush',
        'finishFlush',
        'maxOutputLength',
        'highWaterMark',
        'readableHighWaterMark',
        'writableHighWaterMark',
        'encoding',
        'decodeStrings',
        'emitClose',
        'autoDestroy',
        'signal'
      ].includes(name)
    ) {
      unsupported(`option ${name}`);
    }
  }
  if (value.emitClose === false) {
    unsupported('emitClose:false');
  }
  if (value.decodeStrings === false) {
    unsupported('decodeStrings:false');
  }
  const window = integer(value.windowBits, 15, 8, 15, 'windowBits');
  return {
    raw: value,
    window,
    level: integer(value.level, -1, -1, 9, 'level'),
    memory: integer(value.memLevel, 8, 1, 9, 'memLevel'),
    strategy: integer(value.strategy, 0, 0, 4, 'strategy'),
    chunk: integer(value.chunkSize, 16384, 64, 1048576, 'chunkSize'),
    flush: flushKind(value.flush, 0),
    finish: flushKind(value.finishFlush, 4),
    max: integer(value.maxOutputLength, 64 * 1048576, 1, 64 * 1048576, 'maxOutputLength')
  };
}

export { constants, error, unsupported, options, flushKind };
