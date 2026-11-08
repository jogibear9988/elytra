/** Return only native-confirmed compositor policy; absence means unknown, never GPU success. */
export default function createGPU(native, isReady, isExited, error, unsupported) {
  function alive() {
    if (isExited()) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Application has exited');
    }
  }

  function getFeatureStatus() {
    alive();
    const read = native.gpuFeatureStatus;
    if (typeof read !== 'function') {
      return unsupported('app.getGPUFeatureStatus');
    }
    alive();
    const raw = read.call(native);
    alive();
    try {
      if (typeof raw !== 'string' || raw.length > 1024) {
        throw Error('Invalid GPU snapshot');
      }
      const snapshot = JSON.parse(raw);
      if (!snapshot || typeof snapshot !== 'object' || Array.isArray(snapshot)) {
        throw Error('Invalid GPU snapshot');
      }
      const keys = Object.keys(snapshot);
      if (keys.length === 0) {
        return {};
      }
      if (
        keys.length !== 2 ||
        !keys.includes('gpu_compositing') ||
        !keys.includes('rasterization') ||
        snapshot.gpu_compositing !== 'disabled_software' ||
        snapshot.rasterization !== 'disabled_software'
      ) {
        throw Error('Unconfirmed GPU features');
      }
      return { gpu_compositing: snapshot.gpu_compositing, rasterization: snapshot.rasterization };
    } catch {
      throw error('ERR_FIREFLY_GPU_DATA', 'Invalid native GPU status snapshot');
    }
  }

  function disableHardwareAcceleration() {
    alive();
    if (isReady()) {
      throw error(
        'ERR_FIREFLY_GPU_TOO_LATE',
        'Hardware acceleration must be disabled before ready'
      );
    }
    const disable = native.gpuDisableHardwareAcceleration;
    if (typeof disable !== 'function') {
      return unsupported('app.disableHardwareAcceleration');
    }
    alive();
    if (isReady()) {
      throw error(
        'ERR_FIREFLY_GPU_TOO_LATE',
        'Hardware acceleration must be disabled before ready'
      );
    }
    disable.call(native);
    alive();
  }

  async function getInfo(type) {
    alive();
    if (!['basic', 'complete'].includes(type)) {
      throw error('ERR_INVALID_ARG_VALUE', 'GPU info type must be basic or complete');
    }
    return unsupported('app.getGPUInfo');
  }

  return { getFeatureStatus, disableHardwareAcceleration, getInfo };
}
