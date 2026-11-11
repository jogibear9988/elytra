/** App-domain recent files use the native document controller; they never open a file. */
export default function createRecentDocuments(native, isReady, isExited, error, unsupported) {
  const bytes = Buffer.byteLength.bind(Buffer);
  let stopped = false;

  function alive() {
    if (stopped || isExited()) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Recent documents are unavailable after app exit');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Recent documents require app readiness');
    }
  }

  function invoke(name, args) {
    alive();
    const operation = native[name];
    if (typeof operation !== 'function') {
      return unsupported('app recent documents');
    }
    alive();
    operation.apply(native, args);
    alive();
  }

  return {
    api: {
      clearRecentDocuments(...args) {
        alive();
        if (args.length) {
          throw error('ERR_INVALID_ARG_TYPE', 'clearRecentDocuments expects no arguments');
        }
        invoke('recentDocumentsClear', []);
      },

      addRecentDocument(...args) {
        alive();
        const value = args[0];
        if (args.length !== 1 || typeof value !== 'string') {
          throw error('ERR_INVALID_ARG_TYPE', 'addRecentDocument expects one path string');
        }
        if (!value || value.includes('\0') || value.length > 32768 || bytes(value) > 32768) {
          throw error(
            'ERR_INVALID_ARG_VALUE',
            'Recent document path is empty, invalid or too long'
          );
        }
        for (let i = 0; i < value.length; ++i) {
          const unit = value.charCodeAt(i);
          if (unit >= 0xd800 && unit <= 0xdbff) {
            const low = value.charCodeAt(++i);
            if (!(low >= 0xdc00 && low <= 0xdfff)) {
              throw error(
                'ERR_INVALID_ARG_VALUE',
                'Recent document path has an unpaired surrogate'
              );
            }
          } else if (unit >= 0xdc00 && unit <= 0xdfff) {
            throw error('ERR_INVALID_ARG_VALUE', 'Recent document path has an unpaired surrogate');
          }
        }
        invoke('recentDocumentsAdd', [value]);
      }
    },

    stop() {
      if (!stopped) {
        stopped = true;
        native.recentDocumentsStop?.();
      }
    }
  };
}
