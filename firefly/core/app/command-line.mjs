/** Main-only native engine command line, deliberately separate from process.argv. */
export default function createCommandLine(native, error, unsupported) {
  function name(value) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'Switch name must be a string');
    }
    if (!/^[a-z0-9][a-z0-9-]{0,127}$/.test(value)) {
      throw error(
        'ERR_INVALID_ARG_VALUE',
        'Switch name must be lowercase ASCII without leading dashes'
      );
    }
    return value;
  }

  return Object.freeze({
    hasSwitch(key) {
      return native.commandLineRead(name(key), true);
    },

    getSwitchValue(key) {
      return native.commandLineRead(name(key), false);
    },

    appendSwitch(key, value = '') {
      name(key);
      if (typeof value !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Switch value must be a string');
      }
      if (value.length > 32768 || value.includes('\0')) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid switch value');
      }
      native.commandLineChange(key, value);
    },

    removeSwitch(key) {
      native.commandLineChange(name(key), null);
    },

    appendArgument() {
      unsupported('commandLine.appendArgument');
    }
  });
}
