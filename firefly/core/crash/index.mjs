/** Explicit local-only crash reporting; never claim an upload/minidump backend. */
export default function createCrashReporter(native, error, unsupported) {
  let started = false;

  function object(value, description) {
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', `${description} must be an object`);
    }
  }

  function annotations(value = {}) {
    object(value, 'Crash annotations');
    const result = Object.create(null);
    const keys = Reflect.ownKeys(value);
    if (keys.length > 64) {
      throw error('ERR_OUT_OF_RANGE', 'At most 64 annotations per group');
    }
    for (const key of keys) {
      if (typeof key !== 'string' || !/^[A-Za-z0-9_.-]{1,39}$/.test(key)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid annotation key');
      }
      const text = value[key];
      if (typeof text !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Annotation values must be strings');
      }
      if (text.length > 2048) {
        throw error('ERR_OUT_OF_RANGE', 'Crash annotation value exceeds budget');
      }
      result[key] = text;
    }
    return result;
  }

  function requireStart() {
    if (!started) {
      throw error('ERR_FIREFLY_NOT_READY', 'Start crashReporter first');
    }
  }

  return {
    start(options) {
      if (started) {
        throw error('ERR_FIREFLY_CRASH_REPORTER_STARTED', 'Crash reporter already started');
      }
      object(options, 'Crash options');
      const allowed = new Set([
        'uploadToServer',
        'submitURL',
        'productName',
        'companyName',
        'ignoreSystemCrashHandler',
        'rateLimit',
        'compress',
        'extra',
        'globalExtra'
      ]);
      for (const key of Reflect.ownKeys(options)) {
        if (!allowed.has(key)) {
          unsupported('crashReporter option');
        }
      }
      for (const key of ['uploadToServer', 'ignoreSystemCrashHandler', 'rateLimit', 'compress']) {
        if (options[key] !== undefined && typeof options[key] !== 'boolean') {
          throw error('ERR_INVALID_ARG_TYPE', `${key} must be a boolean`);
        }
      }
      if (options.uploadToServer !== false) {
        unsupported('crash report uploads; set uploadToServer: false');
      }
      if (options.ignoreSystemCrashHandler === true) {
        unsupported('suppressing the operating-system crash handler');
      }
      if (options.submitURL !== undefined && typeof options.submitURL !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'submitURL must be a string');
      }
      // compress/rateLimit/submitURL affect uploads only. Uploading is disabled and cannot be enabled.
      const productName = options.productName === undefined ? 'Firefly' : options.productName;
      const companyName = options.companyName === undefined ? '' : options.companyName;
      for (const text of [productName, companyName]) {
        if (typeof text !== 'string') {
          throw error('ERR_INVALID_ARG_TYPE', 'Product/company names must be strings');
        }
        if (text.length > 256) {
          throw error('ERR_OUT_OF_RANGE', 'Crash name exceeds metadata budget');
        }
      }
      const wire = JSON.stringify({
        productName,
        companyName,
        extra: annotations(options.extra),
        globalExtra: annotations(options.globalExtra)
      });
      if (wire.length > 16 * 1024) {
        throw error('ERR_OUT_OF_RANGE', 'Crash metadata exceeds budget');
      }
      native.crashStart(wire);
      started = true;
    },

    getUploadToServer() {
      requireStart();
      return false;
    },

    setUploadToServer(value) {
      if (typeof value !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'Upload preference must be boolean');
      }
      if (value) {
        unsupported('crash report uploads');
      }
    },

    getParameters() {
      requireStart();
      return JSON.parse(native.crashParameters());
    }
  };
}
