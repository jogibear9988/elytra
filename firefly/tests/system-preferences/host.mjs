/** Actual Mite/factory/Host; only native preferences/notification sources are controlled. */
import native from 'preferences-test';
import createSystemPreferences from '../../core/system-preferences/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function throws(callback, code) {
  let caught;
  try {
    callback();
  } catch (error) {
    caught = error;
  }
  check(
    caught && (code === undefined ? typeof caught.code === 'string' : caught.code === code),
    'Expected explicit failure ' + code + ', got ' + caught
  );
  return caught;
}

const failure = (code, message) => Object.assign(Error(message), { code });

const component = createSystemPreferences(
  native,
  () => false,
  failure,
  (name) => {
    throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
  }
);
const preferences = component.systemPreferences;
const frames = [];

native.setHandler((json) => {
  const frame = JSON.parse(json);
  frames.push(frame);
  // Malformed native source cases below are direct Host subscriptions. Observe
  // their terminal frames without throwing through an application callback.
  if (!frame.code) {
    component.dispatch(frame);
  }
});

const turn = () => new Promise((resolve) => setTimeout(resolve, 1));

const watchdog = setTimeout(() => {
  throw Error('Preferences component probe timed out');
}, 5000);
check(native.created() === 0, 'Import never constructs provider');
native.preferencesUnsubscribe(2147483647, 'local');
for (const id of [-1, -0, 0.5, NaN, Infinity, '0', 2147483648]) {
  throws(() => native.preferencesSubscribe(id, 'local', 'test'));

  if (id === -1) {
    native.preferencesUnsubscribe(id, 'local');
  } else {
    throws(() => native.preferencesUnsubscribe(id, 'local'));
  }
}
for (const name of [undefined, 1, {}, '\0', '\ud800', 'x'.repeat(4097)]) {
  throws(() => native.preferencesSubscribe(0, 'local', name));
}

throws(() => native.preferencesSubscribe(0, 'invalid', 'test'));

throws(() => native.preferencesSetDefault('test', 'boolean', '"wrong"'));

throws(() => native.preferencesGetDefault('test', 'unsupported'));

check(native.created() === 0, 'Invalid native admission leaves provider uncreated');

preferences.setUserDefault('controlled-string', 'string', 'Grüße 日本語');
preferences.setUserDefault('controlled-boolean', 'boolean', true);
check(
  preferences.getUserDefault('controlled-string', 'string') === 'Grüße 日本語',
  'String native JSON round trip'
);
check(
  preferences.getUserDefault('controlled-boolean', 'boolean') === true,
  'Boolean native JSON round trip'
);
for (const type of ['camera', 'microphone', 'screen']) {
  check(
    preferences.getMediaAccessStatus(type) === 'denied',
    'Read-only native media status conversion'
  );
}
for (const mode of [1, 2, 3, 4, 6]) {
  native.control(mode);

  throws(
    () => preferences.getUserDefault('controlled-string', 'string'),
    'ERR_FIREFLY_SYSTEM_PREFERENCES_DATA'
  );
}
native.control(5);
const nativeFailure = throws(
  () => preferences.getUserDefault('controlled-string', 'string'),
  'ERR_CONTROLLED_PREFERENCES'
);
check(
  nativeFailure.message === 'Native failure: Grüße 日本語',
  'Native failure message preserves UTF-8'
);
native.control(2);

throws(() => preferences.getMediaAccessStatus('camera'), 'ERR_FIREFLY_SYSTEM_PREFERENCES_DATA');

let calls = 0;
let argumentsSeen;

function ownedSubscription(name = 'controlled') {
  const payload = { marker: 'callback-owned' };

  const callback = (...args) => {
    check(payload.marker === 'callback-owned', 'Active callback closure survives GC');
    calls++;
    argumentsSeen = args;
  };

  const index = native.count();
  return {
    id: preferences.subscribeNotification(name, callback),
    weak: new WeakRef(callback),
    index
  };
}

async function collected(weak) {
  for (let attempt = 0; attempt < 8; attempt++) {
    await turn();
    native.collect();
    if (weak.deref() === undefined) {
      return true;
    }
  }
  return false;
}

let owned = ownedSubscription();
check(owned.id === 0, 'First public subscription ID is zero');
await turn(); // End WeakRef construction's implicit keep-alive job before collection.
native.collect();
check(typeof owned.weak.deref() === 'function', 'Active callback retained by actual factory');
native.emit(owned.index, 0);
check(calls === 0, 'Worker callback never enters JS synchronously');
await turn();
check(calls === 1 && argumentsSeen[0] === 'controlled', 'Worker event reaches exact callback');
check(
  argumentsSeen[1].nested.enabled &&
    argumentsSeen[1].unicode === 'Grüße 日本語' &&
    argumentsSeen[2] === 'ControlledSender',
  'Native callback object arguments preserve structure'
);
const beforeWrongKind = native.unsubscribed();
preferences.unsubscribeLocalNotification(owned.id);
check(native.unsubscribed() === beforeWrongKind, 'Wrong-family unsubscribe is a no-op');
native.emit(owned.index, 0);
await turn();
check(calls === 2, 'Wrong family cannot retire active callback');
native.emit(owned.index, 7);
await turn();
check(
  calls === 3 && argumentsSeen[1].value === '\0',
  'JSON userInfo preserves embedded NUL strings'
);
preferences.unsubscribeNotification(owned.id);
native.emit(owned.index, 0);
await turn();
check(calls === 3, 'Unsubscribe revokes late worker notifications');
check(await collected(owned.weak), 'Retired callback is actually collectable');
preferences.unsubscribeNotification(2147483647);

let syncCalls = 0;
const sync = preferences.subscribeLocalNotification('sync', () => syncCalls++);
check(syncCalls === 0, 'Synchronous provider event deferred until admission');
await turn();
check(syncCalls === 1, 'Synchronous provider event delivered once after admission');
preferences.unsubscribeLocalNotification(sync);
const beforeFailed = frames.length;

throws(
  () =>
    preferences.subscribeLocalNotification('sync-fail', () => {
      throw Error('Failed admission callback escaped');
    }),
  'ERR_CONTROLLED_PREFERENCES'
);

await turn();
check(frames.length === beforeFailed, 'Failed admission revokes its early queued notification');
let workspaceCalls = 0;
const workspaceIndex = native.count();
const workspace = preferences.subscribeWorkspaceNotification(null, (name, info, object) => {
  check(
    name === 'ControlledName' && info.nested.enabled && object === 'ControlledSender',
    'Null name passes native catch-all metadata'
  );
  workspaceCalls++;
});
native.emit(workspaceIndex, 0);
await turn();
check(workspaceCalls === 1, 'Workspace native family supported');
preferences.unsubscribeWorkspaceNotification(workspace);
owned = ownedSubscription('stop-owned');

for (const mode of [1, 2, 3, 4, 5, 6, 8, 9]) {
  const index = native.count();
  native.preferencesSubscribe(100 + mode, 'local', 'bad-native-data');
  const before = frames.length;
  native.emit(index, mode);
  await turn();
  check(
    frames.length === before + 1 && frames.at(-1).code,
    'Malformed native event produces explicit terminal failure'
  );
  native.emit(index, 0);
  await turn();
  check(
    frames.length === before + 1,
    'Malformed native source is retired before forwarding failure'
  );
}
const overflowIndex = native.count();
native.preferencesSubscribe(150, 'local', 'overflow-only');
const beforeOverflow = frames.length;
for (let count = 0; count < 65; count++) {
  native.emit(overflowIndex, 0);
}
check(frames.length === beforeOverflow, 'Native burst remains queued outside JS');
await turn();
check(
  frames.length === beforeOverflow + 1 &&
    frames.at(-1).code === 'ERR_FIREFLY_SYSTEM_PREFERENCES_DATA',
  'Overflow retires source and drops queued normal events'
);
native.emit(overflowIndex, 0);
await turn();
check(frames.length === beforeOverflow + 1, 'Overflow source cannot resurrect');

const quotaStart = native.count();
for (let id = 200; id < 263; id++) {
  native.preferencesSubscribe(id, 'distributed', 'quota-only');
}

throws(() => native.preferencesSubscribe(263, 'local', 'quota-only'));

for (let id = 200; id < 263; id++) {
  native.preferencesUnsubscribe(id, 'distributed');
}

throws(() => native.preferencesSubscribe(200, 'distributed', 'reused-id'));

const beforeQuota = frames.length;
for (let index = quotaStart; index < quotaStart + 63; index++) {
  native.emit(index, 0);
}
await turn();
check(frames.length === beforeQuota, 'Retired native quota batch cannot deliver');

native.emit(owned.index, 0);
const beforeStop = frames.length;
const callsBeforeStop = calls;
component.stop();
native.collect();
native.emit(owned.index, 0);
await turn();
check(
  frames.length === beforeStop && calls === callsBeforeStop,
  'Stop revokes queued and late worker events'
);
check(await collected(owned.weak), 'Stopped factory releases callback roots');

throws(() => preferences.getUserDefault('controlled-string', 'string'), 'ERR_FIREFLY_APP_EXITED');

clearTimeout(watchdog);
console.log(
  'PASS real preferences factory/exact Host typed JSON, worker/sync notifications, callback GC, quotas and shutdown'
);
