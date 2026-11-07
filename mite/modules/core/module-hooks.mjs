/** Realm-local synchronous resolve chains. Native code retains this dispatcher, not user closures. */
export default function createResolveHooks(host) {
  const hooks = [];

  function failure(code, message, Type = Error) {
    const error = new Type(message);
    error.code = code;
    return error;
  }

  function registerHooks(options) {
    if (options == null) {
      throw new TypeError('registerHooks requires an options object');
    }
    const { resolve, load } = options;
    for (const [name, value] of [
      ['resolve', resolve],
      ['load', load]
    ]) {
      if (value != null && typeof value !== 'function') {
        throw failure('ERR_INVALID_ARG_TYPE', `${name} must be a function`, TypeError);
      }
    }
    if (load != null) {
      throw failure(
        'ERR_MITE_UNSUPPORTED_API',
        'Synchronous module load hooks are not implemented'
      );
    }
    // Remove the loader's reference on deregistration; active chains and retained handles own theirs.
    const record = { resolve };
    if (resolve) {
      if (hooks.length >= 64) {
        throw failure(
          'ERR_MITE_RESOLVE_HOOK_LIMIT',
          'At most 64 active resolve hooks are supported'
        );
      }
      hooks.push(record);
    }
    return {
      deregister() {
        const index = hooks.indexOf(record);
        if (index !== -1) {
          hooks.splice(index, 1);
        }
      }
    };
  }

  function dispatch(specifier, parentURL, require, importAttributes) {
    if (!hooks.length) {
      return undefined;
    }
    const context = {
      parentURL,
      conditions: require ? ['require', 'node', 'node-addons'] : ['node', 'import', 'node-addons']
    };
    if (!require) {
      context.importAttributes = importAttributes;
    }
    // Snapshot the chain so registration/removal cannot invalidate an active traversal.
    const chain = hooks.map((record) => record.resolve);

    function invoke(index, request, input) {
      if (typeof request !== 'string') {
        throw failure('ERR_INVALID_ARG_TYPE', 'Module specifier must be a string', TypeError);
      }
      if (index < 0) {
        return host.defaultResolve(request, input, require);
      }
      let called = false;

      const nextResolve = (nextRequest, changes) => {
        called = true;
        return invoke(
          index - 1,
          nextRequest,
          changes === undefined ? input : { ...input, ...changes }
        );
      };

      const result = Reflect.apply(chain[index], undefined, [request, input, nextResolve]);
      if (result === null || typeof result !== 'object' || typeof result.then === 'function') {
        throw failure(
          'ERR_INVALID_RETURN_VALUE',
          'Resolve hook must synchronously return an object',
          TypeError
        );
      }
      if (!called && result.shortCircuit !== true) {
        throw failure(
          'ERR_INVALID_RETURN_PROPERTY_VALUE',
          'Resolve hook must call nextResolve or shortCircuit',
          TypeError
        );
      }
      if (typeof result.url !== 'string') {
        throw failure(
          'ERR_INVALID_RETURN_PROPERTY_VALUE',
          'Resolve hook url must be a string',
          TypeError
        );
      }
      // Native loading validates the final URL, format and effective import attributes.
      return result;
    }

    return invoke(chain.length - 1, specifier, context);
  }

  host.setResolveDispatcher(dispatch);
  return registerHooks;
}
