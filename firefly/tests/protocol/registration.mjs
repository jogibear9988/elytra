/** Actual public ESM factory plus native origin publication; each mode is a fresh process. */
import createProtocol from '../../core/protocol/index.mjs';
import * as native from 'registration-test';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  try {
    action();
  } catch (failure) {
    check(failure.code === code, `Expected ${code}, got ${failure.code}: ${failure}`);
    return;
  }
  throw Error(`Expected ${code}`);
}

let ready = false;
const { protocol } = createProtocol(native, () => ready, error, unsupported);
const input = [
  { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true } }
];
const mode = process.argv[2];
if (mode === 'sealed') {
  native.seal();

  fails(
    () => protocol.registerSchemesAsPrivileged(input),
    'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED'
  );

  check(native.configuration() === null, 'sealed default must remain undeclared');
} else if (mode === 'empty') {
  protocol.registerSchemesAsPrivileged([]);
  check(JSON.parse(native.configuration()).schemes.length === 0, 'empty publication');

  fails(
    () => protocol.registerSchemesAsPrivileged(input),
    'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED'
  );
} else if (mode === 'ready') {
  ready = true;

  fails(() => protocol.registerSchemesAsPrivileged(input), 'ERR_FIREFLY_PROTOCOL_TOO_LATE');

  check(native.configuration() === null, 'ready failure must not publish');
} else if (mode === 'publish') {
  const invalid = [
    null,
    {},
    [null],
    [{ scheme: 1 }],
    [{ scheme: 'APP' }],
    [{ scheme: 'http' }],
    [{ scheme: 'app:' }],
    [{ scheme: 'app', unknown: true }],
    [{ scheme: 'app', privileges: null }],
    [{ scheme: 'app', privileges: { secure: 1 } }],
    [{ scheme: 'app' }, { scheme: 'app' }],
    [{ scheme: 'app', privileges: { typo: false } }],
    Array.from({ length: 33 }, (_, i) => ({ scheme: `app${i}` }))
  ];
  for (const record of invalid) {
    let rejected = false;
    try {
      protocol.registerSchemesAsPrivileged(record);
    } catch {
      rejected = true;
    }
    check(rejected && native.configuration() === null, 'invalid batch published state');
  }
  for (const key of ['bypassCSP', 'allowServiceWorkers', 'corsEnabled', 'stream', 'codeCache']) {
    fails(
      () =>
        protocol.registerSchemesAsPrivileged([
          input[0],
          { scheme: 'other', privileges: { [key]: true } }
        ]),
      'ERR_FIREFLY_UNSUPPORTED_API'
    );

    check(native.configuration() === null, `unsupported ${key} partially published`);
  }

  fails(
    () =>
      protocol.registerSchemesAsPrivileged([{ scheme: 'opaque', privileges: { secure: true } }]),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );

  fails(
    () =>
      protocol.registerSchemesAsPrivileged([
        { scheme: 'opaque-fetch', privileges: { supportFetchAPI: true } }
      ]),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
  // The native boundary also refuses direct malformed/oversized records independently.
  for (const wire of [
    '{}',
    '{"schema":2,"schemes":[]}',
    '{"schema":1,"schemes":[{"scheme":"http","standard":true,"secure":true}]}'
  ]) {
    fails(() => native.registerSchemes(wire), 'ERR_INVALID_ARG_VALUE');

    check(native.configuration() === null, 'native validation published state');
  }

  fails(() => native.registerSchemes('x'.repeat(8193)), 'ERR_OUT_OF_RANGE');

  fails(() => native.registerSchemes('ä'.repeat(4097)), 'ERR_OUT_OF_RANGE');

  fails(() => native.registerSchemes(null), 'ERR_INVALID_ARG_TYPE');

  protocol.registerSchemesAsPrivileged([
    ...input,
    { scheme: 'insecure', privileges: { standard: true, secure: false, codeCache: false } },
    { scheme: 'opaque' }
  ]);
  check(
    native.origin('app://EXAMPLE.test:8012/path') === 'app://example.test:8012',
    'real custom origin'
  );
  check(native.secure('app://example.test/path'), 'real secure scheme');
  check(!native.secure('insecure://example.test/path'), 'insecure standard scheme');
  check(native.origin('opaque://example.test/path') === 'null', 'nonstandard stays opaque');
  check(
    native.origin('https://example.test/path') === 'https://example.test',
    'builtin origin intact'
  );
  const config = JSON.parse(native.configuration());
  check(config.schema === 1 && config.schemes.length === 3, 'helper snapshot');
  input[0].scheme = 'mutated';
  check(JSON.parse(native.configuration()).schemes[0].scheme === 'app', 'snapshot owns its data');

  fails(() => protocol.registerSchemesAsPrivileged([]), 'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED');
  // A second factory cannot bypass the native once-only guard.
  fails(
    () =>
      createProtocol(native, () => false, error, unsupported).protocol.registerSchemesAsPrivileged(
        []
      ),
    'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED'
  );

  native.seal();
  check(native.secure('app://example.test/path'), 'launch snapshot preserves policy');
} else {
  throw Error('Unknown registration test mode');
}
console.log(`PASS public scheme registration/native origin ${mode}`);
