/** Package-root discovery using the real public stack API, not a guessed caller path. */
export function discoverCallerFile() {
  const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');
  try {
    Error.prepareStackTrace = (_error, sites) => sites;

    const target = {};
    Error.captureStackTrace(target, discoverCallerFile);
    for (const site of target.stack) {
      const filename = site.getFileName();
      if (filename && !filename.endsWith('/bindings-helper.mjs')) {
        return filename;
      }
    }
    throw new Error('No actual caller source available for package-root discovery');
  } finally {
    if (original) {
      Object.defineProperty(Error, 'prepareStackTrace', original);
    } else {
      delete Error.prepareStackTrace;
    }
  }
}
