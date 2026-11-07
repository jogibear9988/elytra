/** JS argument checks only; native parsing/effects are tested separately. */
import createCommandLine from '../../core/app/command-line.mjs';

const error = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function fails(action, code) {
  let actual;
  try {
    action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

let change;
const line = createCommandLine(
  {
    commandLineRead(name, presence) {
      return presence ? name === 'present' : name === 'present' ? 'value' : '';
    },

    commandLineChange(name, value) {
      change = [name, value];
    }
  },
  error,
  unsupported
);
for (const name of ['', '--lang', 'MiXeD', 'bad=value', 'a b', 'a\0', 'ü', 'x'.repeat(129)]) {
  fails(() => line.hasSwitch(name), 'ERR_INVALID_ARG_VALUE');
}
for (const value of [null, 1, {}, []]) {
  fails(() => line.getSwitchValue(value), 'ERR_INVALID_ARG_TYPE');
}
check(line.hasSwitch('present') && line.getSwitchValue('present') === 'value', 'native reads');
check(!line.hasSwitch('absent') && line.getSwitchValue('absent') === '', 'absent reads');

fails(() => line.appendSwitch('lang', false), 'ERR_INVALID_ARG_TYPE');

fails(() => line.appendSwitch('lang', 'a\0'), 'ERR_INVALID_ARG_VALUE');

fails(() => line.appendArgument('anything'), 'ERR_FIREFLY_UNSUPPORTED_API');

check(line.appendSwitch('lang', 'de-DE') === undefined && change[1] === 'de-DE', 'append contract');
check(line.removeSwitch('lang') === undefined && change[1] === null, 'remove contract');
check(Object.isFrozen(line), 'immutable command-line facade');
console.log('PASS command-line JS argument validation and native request contracts');
