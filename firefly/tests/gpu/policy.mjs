/** Actual GPU policy factory with controlled native snapshots; no hardware/device or renderer claim. */
import create from '../../core/app/gpu.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
  return caught;
}

function fixture() {
  const state = { ready: false, exited: false, status: {}, disables: 0 };
  const native = {
    gpuFeatureStatus() {
      state.onQuery?.();
      if (state.failure) {
        throw state.failure;
      }
      return Object.hasOwn(state, 'raw') ? state.raw : JSON.stringify(state.status);
    },

    gpuDisableHardwareAcceleration() {
      state.disables++;
      state.onDisable?.();
      if (state.failure) {
        throw state.failure;
      }
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const gpu = create(
    native,
    () => state.ready,
    () => state.exited,
    error,
    (name) => {
      throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
    }
  );
  return { state, native, gpu, error };
}

const CPU = { gpu_compositing: 'disabled_software', rasterization: 'disabled_software' };
const DATA = 'ERR_FIREFLY_GPU_DATA';
const EXITED = 'ERR_FIREFLY_APP_EXITED';

{
  const { gpu, state } = fixture();
  check(
    JSON.stringify(gpu.getFeatureStatus()) === '{}',
    'No native device/policy knowledge means unknown'
  );
  state.ready = true;
  check(JSON.stringify(gpu.getFeatureStatus()) === '{}', 'Readiness cannot invent GPU knowledge');
  state.status = { ...CPU };
  check(JSON.stringify(gpu.getFeatureStatus()) === JSON.stringify(CPU), 'Known CPU backend status');
  console.log('PASS pre-ready and ready unknown status versus actual selected CPU policy');
}

{
  const { gpu, state } = fixture();
  state.status = { ...CPU };
  const first = gpu.getFeatureStatus();
  const second = gpu.getFeatureStatus();
  check(first !== second, 'Fresh snapshots');
  first.gpu_compositing = 'enabled';
  first.fake = 'forged';
  check(
    JSON.stringify(gpu.getFeatureStatus()) === JSON.stringify(CPU),
    'External mutation cannot alter native policy'
  );
  state.status = {};
  check(
    !Object.keys(gpu.getFeatureStatus()).length,
    'Fresh observations do not retain stale CPU flags'
  );
  console.log('PASS native snapshot isolation fresh observation and no retained status forgery');
}

{
  const { gpu, state } = fixture();
  gpu.disableHardwareAcceleration();
  gpu.disableHardwareAcceleration();
  check(state.disables === 2, 'Early disable reaches real native policy owner');
  check(
    !Object.keys(gpu.getFeatureStatus()).length,
    'Request alone does not claim CPU backend selection'
  );
  state.ready = true;

  fails(() => gpu.disableHardwareAcceleration(), 'ERR_FIREFLY_GPU_TOO_LATE');

  check(state.disables === 2, 'Late request cannot reach native provider');
  console.log('PASS early acceleration policy admission and explicit late mutation rejection');
}

{
  const { gpu, state } = fixture();
  for (const raw of [
    'null',
    '[]',
    'false',
    '1',
    '"text"',
    '{',
    undefined,
    {},
    5,
    '{"gpu_compositing":true}',
    '{"gpu_compositing":"enabled"}',
    '{"rasterization":"disabled_software","unknown":"disabled_software"}',
    '{"__proto__":"disabled_software"}'
  ]) {
    state.raw = raw;

    fails(() => gpu.getFeatureStatus(), DATA);
  }
  console.log('PASS malformed native JSON unknown fields and invented enabled capability rejected');
}

{
  const { gpu, state, error, native } = fixture();
  const failure = error('ERR_NATIVE_GPU', 'Native policy failure');
  state.failure = failure;
  check(
    fails(() => gpu.getFeatureStatus(), failure.code) === failure,
    'Native query error preserved'
  );
  check(
    fails(() => gpu.disableHardwareAcceleration(), failure.code) === failure,
    'Native mutation error preserved'
  );
  delete state.failure;
  delete native.gpuFeatureStatus;

  fails(() => gpu.getFeatureStatus(), 'ERR_FIREFLY_UNSUPPORTED_API');

  delete native.gpuDisableHardwareAcceleration;

  fails(() => gpu.disableHardwareAcceleration(), 'ERR_FIREFLY_UNSUPPORTED_API');

  let unsupported;
  try {
    await gpu.getInfo('complete');
  } catch (failure) {
    unsupported = failure;
  }
  check(
    unsupported?.code === 'ERR_FIREFLY_UNSUPPORTED_API',
    'Detailed device info explicitly unsupported'
  );
  console.log(
    'PASS genuine native errors missing providers and detailed device information unavailable'
  );
}

{
  for (const method of ['getFeatureStatus', 'disableHardwareAcceleration']) {
    const f = fixture();
    f.state.exited = true;

    fails(() => f.gpu[method](), EXITED);
  }
  const query = fixture();

  query.state.onQuery = () => {
    query.state.exited = true;
  };

  fails(() => query.gpu.getFeatureStatus(), EXITED);

  const disable = fixture();

  disable.state.onDisable = () => {
    disable.state.exited = true;
  };

  fails(() => disable.gpu.disableHardwareAcceleration(), EXITED);

  console.log('PASS exited and reentrant native lifecycle cannot return stale policy success');
}
console.log('DONE GPU policy');
