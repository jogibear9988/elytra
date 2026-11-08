/** Utility processes own the same networking policy without initializing the desktop or Qt. */
import createNet from './index.mjs';

export default function createUtilityNet(native, transport) {
  let exited = false;

  const error = (code, message) => Object.assign(new Error(message), { code });

  const unsupported = (name) => {
    throw error('ERR_FIREFLY_UNSUPPORTED_API', `${name} is not implemented`);
  };

  const service = createNet(
    native,
    () => true,
    () => exited,
    error,
    unsupported,
    transport
  );
  return {
    net: service.net,

    stop() {
      exited = true;
      service.stop();
    }
  };
}
