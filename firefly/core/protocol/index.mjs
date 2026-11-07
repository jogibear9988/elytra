/** Main-only origin registration. File routes and session ownership are separate capabilities. */
export default function createProtocol(native, isReady, error, unsupported) {
  let registered = false;
  const reserved = new Set([
    'http',
    'https',
    'ws',
    'wss',
    'ftp',
    'file',
    'about',
    'blob',
    'data',
    'javascript',
    'resource',
    'chrome',
    'chrome-extension',
    'devtools',
    'webkit',
    'moz-extension',
    'mite',
    'mite-host',
    'firefly'
  ]);
  const privileges = new Set([
    'standard',
    'secure',
    'bypassCSP',
    'allowServiceWorkers',
    'supportFetchAPI',
    'corsEnabled',
    'stream',
    'codeCache'
  ]);

  function object(value, description) {
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', `${description} must be an object`);
    }
  }

  return {
    registerSchemesAsPrivileged(customSchemes) {
      if (isReady()) {
        throw error('ERR_FIREFLY_PROTOCOL_TOO_LATE', 'Register schemes before app is ready');
      }
      if (registered) {
        throw error(
          'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED',
          'Schemes can be registered only once'
        );
      }
      if (!Array.isArray(customSchemes)) {
        throw error('ERR_INVALID_ARG_TYPE', 'customSchemes must be an array');
      }
      if (customSchemes.length > 32) {
        throw error('ERR_OUT_OF_RANGE', 'At most 32 custom schemes can be registered');
      }
      const seen = new Set();
      const schemes = [];
      for (const entry of customSchemes) {
        object(entry, 'Custom scheme');
        for (const key of Reflect.ownKeys(entry)) {
          if (key !== 'scheme' && key !== 'privileges') {
            throw error('ERR_INVALID_ARG_VALUE', 'Unknown custom scheme option');
          }
        }
        const scheme = entry.scheme;
        if (typeof scheme !== 'string') {
          throw error('ERR_INVALID_ARG_TYPE', 'scheme must be a string');
        }
        if (!/^[a-z][a-z0-9+.-]{0,63}$/.test(scheme) || reserved.has(scheme) || seen.has(scheme)) {
          throw error('ERR_INVALID_ARG_VALUE', 'Scheme must be canonical, custom and unique');
        }
        seen.add(scheme);
        const values = entry.privileges === undefined ? {} : entry.privileges;
        object(values, 'Scheme privileges');
        let standard = false;
        let secure = false;
        for (const key of Reflect.ownKeys(values)) {
          if (!privileges.has(key)) {
            throw error('ERR_INVALID_ARG_VALUE', 'Unknown scheme privilege');
          }
          const value = values[key];
          if (typeof value !== 'boolean') {
            throw error('ERR_INVALID_ARG_TYPE', `Privilege ${key} must be a boolean`);
          }
          if (key === 'standard') {
            standard = value;
          } else if (key === 'secure') {
            secure = value;
          } else if (value) {
            unsupported(`protocol privilege ${key}`);
          }
        }
        if (secure && !standard) {
          unsupported('secure nonstandard protocol origins');
        }
        schemes.push({ scheme, standard, secure });
      }
      // Validate the whole batch before native publication. Failure consumes no registration.
      // Native code independently checks the schema and refuses any post-helper publication.
      native.registerSchemes(JSON.stringify({ schema: 1, schemes }));
      registered = true;
    }
  };
}
