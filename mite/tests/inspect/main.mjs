/** Selected real formatting and prototype custom inspection; no full util.inspect parity claim. */
import { inspect, format, formatWithOptions, types } from 'node:util';
import createInspection from '../../modules/core/inspect.mjs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const actual = process.versions.mite
  ? { inspect, format, formatWithOptions }
  : createInspection({ types });
const custom = Symbol.for('nodejs.util.inspect.custom');
check(actual.inspect.custom === custom, 'Registered custom symbol');
const circular = {};
circular.self = circular;
let calls = 0;
let getterCalls = 0;
class HeadersLike {
  #entries = new Map([
    ['one', 'value'],
    ['two', 'next']
  ]);

  [custom](depth, options, callback) {
    check(
      this instanceof HeadersLike && typeof depth === 'number' && callback === actual.inspect,
      'Custom method receiver/depth/callback'
    );
    check(
      options.colors === false && typeof options.stylize === 'function',
      'Genuine inspection options'
    );
    calls++;
    return 'Headers ' + actual.formatWithOptions(options, this.#entries);
  }
}
const values = [
  undefined,
  null,
  true,
  -0,
  12n,
  'hello',
  "a'b",
  [],
  [1, 'two'],
  { a: 1, b: 'two' },
  { nested: { value: 3 } },
  Object.assign(Object.create(null), { actual: true }),
  new Map([['x', 1]]),
  new Set([1, 'x']),
  circular
];
const rows = values.map((value) => actual.inspect(value));
if (!process.versions.mite) {
  for (let index = 0; index < values.length; index++) {
    check(
      rows[index] === inspect(values[index]),
      'Selected Node format case ' + index + ': ' + rows[index] + ' vs ' + inspect(values[index])
    );
  }
}
const headers = new HeadersLike();
check(
  actual.inspect(headers) === "Headers Map(2) { 'one' => 'value', 'two' => 'next' }",
  'Actual Undici-shaped custom formatter'
);
check(calls === 1, 'Custom prototype data method called once');
const accessor = {
  get guarded() {
    getterCalls++;
    throw Error('Accessor ran');
  }
};
check(
  actual.inspect(accessor) === '{ guarded: [Getter] }' && getterCalls === 0,
  'Default inspection never evaluates own getters'
);
const sentinel = {};
try {
  actual.inspect({
    [custom]() {
      throw sentinel;
    }
  });
  throw Error('Expected hook throw');
} catch (error) {
  check(error === sentinel, 'Custom exception identity preserved');
}
const self = {
  a: 3,

  [custom]() {
    return this;
  }
};
check(actual.inspect(self).includes('a: 3'), 'Self return falls back to real own-data inspection');
check(
  actual.format('%s %d %i %f %j', 'x', 7, 8.9, 2.5, { a: true }) === 'x 7 8 2.5 {"a":true}',
  'Real format substitutions'
);
check(
  actual.inspect({ a: { b: 3 } }, { depth: 0 }) === '{ a: [Object] }',
  'Depth applies to actual child graph'
);
check(actual.inspect({ x: 1 }, { stylize: (text) => text }) === '{ x: 1 }', 'Callable stylization');
console.log(
  'PASS inspect genuine data, Map/Set/cycles, prototype custom receiver/options/callback, getters and formatting'
);
