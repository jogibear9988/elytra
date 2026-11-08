/** Real LibJS factory and exact production Host; native OS ownership alone is controlled. */
import native from 'shortcut-test';
import createGlobalShortcuts from '../../core/global-shortcut/index.mjs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const failure = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const tick = () => new Promise((resolve) => setTimeout(resolve, 10));

let ready = false;
const component = createGlobalShortcuts(
  native,
  () => ready,
  () => false,
  failure,
  unsupported,
  process.platform
);

native.setHandler((frame) => component.dispatch(JSON.parse(frame)));

const shortcut = component.globalShortcut;

const expect = (fn, code) => {
  let actual;
  try {
    fn();
  } catch (error) {
    actual = error;
  }
  check(actual?.code === code, `Expected ${code}, got ${actual?.code}: ${actual?.message}`);
};

if (process.argv.includes('--leave-active')) {
  ready = true;
  check(
    shortcut.register('Ctrl+F17', () => {
      throw Error('Unexpected teardown event');
    }),
    'Outstanding native owner admitted for actual VM shutdown'
  );
  check(native.owned() === 1, 'Native ownership remains outstanding before VM shutdown');
  console.log('PASS globalShortcut outstanding native registration reaches real VM shutdown');
} else {
  check(
    !shortcut.isRegistered('Ctrl+F17') && native.created() === 0,
    'Read-only query before readiness creates no native provider'
  );

  expect(() => shortcut.register('Ctrl+F17', () => {}), 'ERR_FIREFLY_NOT_READY');

  check(native.created() === 0, 'Readiness rejection precedes native provider creation');
  ready = true;

  expect(() => native.globalShortcutRegister(1, '\ud800', 2), 'ERR_INVALID_ARG_VALUE');

  expect(() => native.globalShortcutRegister(1, 'F17\0', 2), 'ERR_INVALID_ARG_VALUE');

  check(native.created() === 0, 'Malformed native key rejected before provider creation');
  for (const token of [-1, -0, 0, 1.5, Infinity, '1', null]) {
    let rejected = false;
    try {
      native.globalShortcutRegister(token, 'F17', 2);
    } catch {
      rejected = true;
    }
    check(
      rejected && native.created() === 0,
      'Native strict token validation before provider creation'
    );
  }
  native.control(1);
  check(!shortcut.register('Ctrl+F17', () => {}), 'Native conflict returns false');
  check(
    !shortcut.isRegistered('Ctrl+F17') && native.owned() === 0,
    'Failed admission owns no callback/registration'
  );
  native.control(2);

  expect(() => shortcut.register('Ctrl+F17', () => {}), 'ERR_CONTROLLED_SHORTCUT');

  let calls = 0;

  function registerRetained() {
    const marker = { message: 'Grüße 日本語' };
    return shortcut.register('Ctrl+F17', () => {
      native.collect();
      check(
        marker.message === 'Grüße 日本語',
        'Native dispatch/GC preserves active callback closure'
      );
      calls++;
    });
  }

  check(registerRetained(), 'Actual native Host admits factory callback');
  const retained = native.count() - 1;
  check(
    !shortcut.register('Control+F17', () => {
      throw Error('Overwritten callback');
    }),
    'Canonical duplicate does not overwrite owner'
  );
  native.collect();
  native.emit(retained);
  check(calls === 0, 'Worker native event is queued, never entered on worker');
  await tick();
  check(calls === 1, 'Queued owner callback survives actual collector');
  native.control(4);
  check(
    !shortcut.registerAll(['Ctrl+F18', 'Ctrl+F19'], () => {}),
    'Second native conflict fails registerAll'
  );
  check(
    shortcut.isRegistered('Ctrl+F17') && !shortcut.isRegistered('Ctrl+F18') && native.owned() === 1,
    'Transactional rollback removes new admissions while retaining prior owner'
  );
  shortcut.unregister('Control+F17');
  native.emit(retained);
  await tick();
  check(calls === 1 && native.owned() === 0, 'Unregister filters late native callback');
  check(
    shortcut.register('Ctrl+F16', () => calls++),
    'Transient release owner admitted'
  );
  const failedRelease = native.count() - 1;
  native.control(5);

  expect(() => shortcut.unregister('Ctrl+F16'), 'ERR_CONTROLLED_RELEASE');

  check(
    !shortcut.isRegistered('Ctrl+F16') && native.owned() === 1,
    'Failed native removal revokes callback but retains ownership'
  );
  native.emit(failedRelease);
  await tick();
  check(calls === 1, 'Failed-release native event cannot reach revoked callback');
  check(
    shortcut.register('Ctrl+F15', () => calls++),
    'Next admission retries exact Host release'
  );
  check(native.owned() === 1, 'Old native handle released before new admission');
  shortcut.unregister('Ctrl+F15');
  check(native.owned() === 0, 'Retried native ownership leaves no leak');
  native.control(3);
  check(
    shortcut.register('Ctrl+F20', () => calls++),
    'Native synchronous provider callback admits successfully'
  );
  check(calls === 1, 'Native synchronous callback still queued past register return');
  await tick();
  check(calls === 2, 'Accepted synchronous provider callback delivered on owner');
  const finalRegistration = native.count() - 1;
  native.emit(finalRegistration); // Queue before shutdown, then revoke before Qt delivery.
  native.destroyReceiver();
  check(
    native.owned() === 0,
    'Receiver destruction revokes native provider before controller destruction'
  );
  component.stop();
  native.emit(finalRegistration);
  native.collect();
  await tick();
  check(
    calls === 2 && native.owned() === 0,
    'Shutdown releases owned registrations and drops late events'
  );

  expect(() => shortcut.register('Ctrl+F17', () => {}), 'ERR_FIREFLY_APP_EXITED');

  console.log(
    'PASS exact globalShortcut Host strict admission, native conflict/rollback, queued worker events, callback GC/reentry and shutdown'
  );
}
