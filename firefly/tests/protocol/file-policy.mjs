/** Deterministic JS policy checks; actual native/helper acceptance is a separate gate. */
import createFiles from '../../core/protocol/files.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  try {
    action();
  } catch (failure) {
    check(failure.code === code, String(failure));
    return;
  }
  throw Error(`Expected ${code}`);
}

let ready = false;
let generation = 0;
const replies = [];
const native = {
  protocolRegisterFile() {
    return String(++generation);
  },

  protocolUnregisterFile() {
    return true;
  },

  protocolFileReply(wire) {
    replies.push(JSON.parse(wire));
    return true;
  }
};
const files = createFiles(
  native,
  () => ready,
  error,
  (name) => name === 'app'
);
const api = files.api;

fails(() => api.registerFileProtocol('app', () => {}), 'ERR_FIREFLY_NOT_READY');

ready = true;

fails(() => api.registerFileProtocol('other', () => {}), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => api.registerFileProtocol('app', null), 'ERR_INVALID_ARG_TYPE');

let reply;
let requests = 0;
check(
  api.registerFileProtocol('app', (request, callback) => {
    check(
      Object.isFrozen(request) && request.method === 'HEAD' && request.headers === undefined,
      'request contract'
    );
    ++requests;
    reply = callback;
  }),
  'registration'
);
check(!api.registerFileProtocol('app', () => {}), 'duplicate');

const request = (token, gen = generation) =>
  files.dispatch('protocol-request', {
    token,
    generation: String(gen),
    scheme: 'app',
    url: 'app://bundle/body',
    method: 'HEAD'
  });

request('1');
check(
  reply({ path: '/owned/body', headers: { 'X-Test': ['one', 'two'] } }) === undefined,
  'callback returns no status'
);
check(
  replies[0].headers.length === 2 && replies[0].headers[1][1] === 'two',
  'ordered header values'
);
check(reply('/owned/other') === undefined && replies.length === 1, 'duplicate callback');
request('2');
files.dispatch('protocol-cancel', { token: '2' });
check(reply('/owned/body') === undefined && replies.length === 1, 'cancelled callback');
request('3');
const old = reply;
api.unregisterProtocol('app');
check(old('/owned/body') === undefined, 'unregistered callback');
check(
  api.registerFileProtocol('app', () => {
    throw Error('must not receive stale generation');
  }),
  'replacement'
);
request('4', 1);
check(replies.at(-1).error && requests === 3, 'generation mismatch');
api.unregisterProtocol('app');
check(
  api.registerFileProtocol('app', () => {
    throw Error('handler failure');
  }),
  'throwing handler'
);
request('5');
check(replies.at(-1).token === '5' && replies.at(-1).error, 'synchronous failure settlement');
api.unregisterProtocol('app');

api.registerFileProtocol('app', async () => {
  throw Error('async failure');
});

request('6');
await Promise.resolve();
await Promise.resolve();
check(replies.at(-1).token === '6' && replies.at(-1).error, 'async failure settlement');
api.unregisterProtocol('app');

api.registerFileProtocol('app', (_request, callback) => {
  reply = callback;
});

for (const response of [
  null,
  { path: '/x', unsupported: true },
  { error: 0 },
  { path: '/x', headers: { X: 1 } }
]) {
  const token = String(replies.length + 100);
  request(token);
  let rejected = false;
  try {
    reply(response);
  } catch {
    rejected = true;
  }
  check(
    rejected && replies.at(-1).token === token && replies.at(-1).error,
    'invalid reply must refuse native request'
  );
  check(reply('/owned/body') === undefined, 'failed response callback retained');
}
console.log(
  'PASS file-handler JS readiness/validation, callback cancellation, generations and thrown/rejected handlers'
);
