/** Real nextTick warning delivery and stderr output; no terminal/color assumptions. */
export default function installWarnings(process) {
  const capture = Error.captureStackTrace;
  const tick = process.nextTick.bind(process);
  let pending = 0;
  let pendingCharacters = 0;

  function invalid(message) {
    throw Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  function text(value, name) {
    if (typeof value !== 'string') {
      invalid(`${name} must be a string`);
    }
    if (value.length > 65536) {
      throw Object.assign(new RangeError('Warning text exceeds 65536 code units'), {
        code: 'ERR_MITE_WARNING_LIMIT'
      });
    }
    return value;
  }

  function onWarning(warning) {
    if (!(warning instanceof Error)) {
      return;
    }
    const deprecation = warning.name === 'DeprecationWarning';
    if (deprecation && process.noDeprecation) {
      return;
    }
    const trace = process.traceProcessWarnings || (deprecation && process.traceDeprecation);
    const body = trace && warning.stack ? String(warning.stack) : String(warning);
    const detail = typeof warning.detail === 'string' ? `\n${warning.detail}` : '';
    process.stderr.write(
      `(${process.release.name}:${process.pid}) ${warning.code ? `[${warning.code}] ` : ''}${body}${detail}\n`
    );
  }

  function emitWarning(warning, type, code, ctor) {
    if (process.noDeprecation && type === 'DeprecationWarning') {
      return;
    }
    let detail;
    if (type !== null && typeof type === 'object' && !Array.isArray(type)) {
      for (const key of Object.keys(type)) {
        if (!['type', 'code', 'detail', 'ctor'].includes(key)) {
          throw Object.assign(new Error(`Warning option ${key} is not implemented`), {
            code: 'ERR_MITE_UNSUPPORTED_API'
          });
        }
      }
      const options = type;
      ctor = options.ctor;
      code = options.code;
      detail = typeof options.detail === 'string' ? text(options.detail, 'detail') : undefined;
      type = options.type || 'Warning';
    } else if (typeof type === 'function') {
      ctor = type;
      code = undefined;
      type = 'Warning';
    }
    if (type !== undefined) {
      text(type, 'type');
    }
    if (typeof code === 'function') {
      ctor = code;
      code = undefined;
    } else if (code !== undefined) {
      text(code, 'code');
    }
    if (ctor !== undefined && typeof ctor !== 'function') {
      invalid('ctor must be a function');
    }
    let characters;
    if (typeof warning === 'string') {
      const message = text(warning, 'warning');
      warning = new Error(message);
      warning.name = type || 'Warning';
      if (code !== undefined) {
        warning.code = code;
      }
      if (detail !== undefined) {
        warning.detail = detail;
      }
      capture(warning, ctor || emitWarning);
      characters =
        message.length + warning.name.length + (code?.length || 0) + (detail?.length || 0);
    } else if (warning instanceof Error) {
      // Retain the exact authored Error, matching Node's identity contract.
      characters = text(String(warning.message), 'message').length;
    } else {
      invalid('warning must be a string or Error');
    }
    if (warning.name === 'DeprecationWarning' && process.noDeprecation) {
      return;
    }
    if (pending >= 256 || pendingCharacters + characters > 1024 * 1024) {
      throw Object.assign(new RangeError('Pending warning budget exceeded'), {
        code: 'ERR_MITE_WARNING_LIMIT'
      });
    }
    pending++;
    pendingCharacters += characters;
    const shouldThrow = warning.name === 'DeprecationWarning' && process.throwDeprecation;

    tick(() => {
      pending--;
      pendingCharacters -= characters;
      if (shouldThrow) {
        throw warning;
      }
      process.emit('warning', warning);
    });
  }

  Object.defineProperty(process, 'emitWarning', {
    value: emitWarning,
    writable: true,
    configurable: true,
    enumerable: true
  });
  process.on('warning', onWarning);
}
