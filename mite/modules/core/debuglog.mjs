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
 * NODE_DEBUG matching and lazy stderr loggers, derived from pinned Node24
 * internal/util/debuglog.js. Formatting uses the existing bounded Console
 * implementation; no V8 inspection, ANSI color or global console replacement.
 */
import createConsoleModule from './console.mjs';

export default function createDebugLog(host) {
  const process = host.process;
  const implementations = new Map();
  const debugEnv = process.env.NODE_DEBUG;
  const expression = debugEnv
    ? new RegExp(
        '^' +
          debugEnv
            .replace(/[|\\{}()[\]^$+?.]/g, '\\$&')
            .replaceAll('*', '.*')
            .replaceAll(',', '$|^') +
          '$',
        'i'
      )
    : undefined;
  let output;
  const upper = Function.call.bind(String.prototype.toUpperCase);
  const apply = Reflect.apply;

  function implementation(enabled, category) {
    if (!implementations.has(category)) {
      let log = () => {};

      if (enabled) {
        if (category === 'HTTP' || category === 'HTTP2') {
          process.emitWarning(
            `Setting the NODE_DEBUG environment variable to '${category.toLowerCase()}' can expose sensitive data (such as passwords, tokens and authentication headers) in the resulting log.`
          );
        }

        log = (...args) => {
          if (!output) {
            // This is a private Console instance, not the global console nor
            // its mutable user wrappers (including parent-process log piping).
            const { Console } = createConsoleModule({ process, types: host.types, console: {} });
            output = new Console({
              stdout: process.stderr,
              stderr: process.stderr,
              ignoreErrors: false,
              colorMode: false
            });
          }
          const prefix = `${category} ${process.pid}: `;
          if (!args.length) {
            return output.error(prefix);
          }
          if (typeof args[0] === 'string') {
            return output.error(prefix + args[0], ...args.slice(1));
          }
          return output.error(prefix + '%O', ...args);
        };
      }
      implementations.set(category, log);
    }
    return implementations.get(category);
  }

  function debuglog(category, callback) {
    let enabled;

    const initialize = () => {
      category = upper(category);
      enabled = expression !== undefined && expression.test(category);
    };

    let debug = (...args) => {
      initialize();
      debug = implementation(enabled, category);
      if (typeof callback === 'function') {
        Object.defineProperty(debug, 'enabled', {
          get: () => enabled,

          configurable: true,
          enumerable: true
        });
        callback(debug);
      }
      return apply(debug, undefined, args);
    };

    let test = () => {
      initialize();

      test = () => enabled;

      return enabled;
    };

    const logger = (...args) => {
      if (enabled === false) {
        return;
      }
      return apply(debug, undefined, args);
    };

    Object.defineProperty(logger, 'enabled', {
      get: () => test(),

      configurable: true,
      enumerable: true
    });
    return logger;
  }

  return debuglog;
}
