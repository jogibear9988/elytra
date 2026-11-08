/** Actual native Host policy; no route transport or protocol.handle acceptance claim. */
import * as native from 'protocol-route-test';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function rejects(fn, code) {
  let caught;
  try {
    fn();
  } catch (error) {
    caught = error;
  }
  check(caught?.code === code, `expected ${code}, received ${caught?.code}`);
}

const state = () => JSON.parse(native.state());

check(state().generation === '0' && !state().active, 'initial policy');
check(native.protocolRegisterFile('app') === '1', 'legacy generation');
check(native.protocolRegisterResponse('app') === false, 'shared kind collision');
check(native.protocolUnregisterFile('app'), 'legacy removal');

rejects(() => native.protocolRegisterFile('file'), 'ERR_FIREFLY_UNSUPPORTED_API');

rejects(() => native.protocolRegisterResponse('https'), 'ERR_FIREFLY_UNSUPPORTED_API');

rejects(() => native.protocolRegisterResponse('FILE'), 'ERR_FIREFLY_UNSUPPORTED_API');

check(native.protocolRegisterResponse('file') === '2', 'actual modern file admission');
check(state().active && state().generation === '1', 'active file policy');
check(native.protocolRegisterResponse('file') === false, 'duplicate file collision');
native.collect();
check(state().policies.length === 1, 'duplicate does not republish policy');
check(native.protocolUnregisterFile('file'), 'shared modern removal');
check(!state().active && state().generation === '2', 'revoked policy generation');
check(!native.protocolUnregisterFile('file'), 'duplicate removal false');
check(native.protocolRegisterResponse('app') === '3', 'modern custom generation');
check(native.protocolRegisterFile('app') === false, 'reverse kind collision');
check(native.protocolUnregisterFile('app'), 'custom modern removal');

const head = {
  schema: 1,
  kind: 'stream-response',
  status: 200,
  length: null,
  head: false,
  bodyNull: false,
  headers: [['X-Bytes', '\x80\xff']]
};
check(native.protocolResponseHead(JSON.stringify({ token: '1', head })) === false, 'stale header');
check(
  native.protocolResponseHead(JSON.stringify({ token: '1', error: true })) === false,
  'stale error'
);
check(native.protocolResponseChunk('1', new Uint8Array([1, 2]), false) === false, 'stale chunk');
check(native.protocolResponseChunk('1', new Uint8Array(0), true) === false, 'stale EOF');
for (const token of ['0', '01', '+1', '-1', '1.0', '18446744073709551616']) {
  rejects(
    () => native.protocolResponseHead(JSON.stringify({ token, head })),
    'ERR_INVALID_ARG_VALUE'
  );

  rejects(
    () => native.protocolResponseChunk(token, new Uint8Array(0), true),
    'ERR_INVALID_ARG_VALUE'
  );
}
for (const invalid of [
  null,
  {},
  { ...head, path: '/private/secret' },
  { ...head, status: 200.5 },
  { ...head, headers: [['X', '€']] },
  { ...head, headers: [['Transfer-Encoding', 'chunked']] }
]) {
  rejects(
    () => native.protocolResponseHead(JSON.stringify({ token: '1', head: invalid })),
    'ERR_INVALID_ARG_VALUE'
  );
}

rejects(() => native.protocolResponseHead(' '.repeat(32768 + 129)), 'ERR_OUT_OF_RANGE');

rejects(() => native.protocolResponseChunk('1', new Uint8Array(65537), false), 'ERR_OUT_OF_RANGE');

rejects(() => native.protocolResponseChunk('1', new Uint16Array(1), false), 'ERR_INVALID_ARG_TYPE');

rejects(() => native.protocolResponseChunk('1', new Uint8Array(1), 1), 'ERR_INVALID_ARG_TYPE');

native.collect();
check(state().policies.length === 2, 'exact policy transitions survived actual GC');
console.log(
  'PASS actual LibJS route Host kind collisions, file policy generations, stale responses, binary bounds and GC; no protocol.handle transport claim'
);
