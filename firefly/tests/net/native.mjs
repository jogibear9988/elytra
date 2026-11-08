/** Ready component context; exact native OS queries, actual Mite transport and real factory. */
import native from 'net-test';
import createNet from '../../core/net/index.mjs';
import exerciseNet from './transport.mjs';

const failure = (code, message) => Object.assign(Error(message), { code });

const component = createNet(
  native,
  () => true,
  () => false,
  failure,
  (name) => {
    throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
  },
  native.transport
);
try {
  let online;
  const deadline = Date.now() + 2000;
  while (typeof online !== 'boolean') {
    try {
      online = component.net.isOnline();
    } catch (error) {
      if (error.code !== 'ERR_FIREFLY_NETWORK_STATE_PENDING' || Date.now() >= deadline) {
        throw error;
      }
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }
  if (typeof component.net.online !== 'boolean' || typeof native.networkHasProxy() !== 'boolean') {
    throw Error('Genuine network state queries');
  }
  native.collect();
  if (process.env.ELYTRA_NET_TEST) {
    const config = JSON.parse(process.env.ELYTRA_NET_TEST);
    await exerciseNet(component.net, { ...config, label: 'native', strictOrigin: true });
    console.log('PASS actual net factory/exact Host/captured Mite HTTP(S) transport');
  }
  console.log('PASS actual net factory/exact Host read-only native availability/proxy snapshots');
} finally {
  component.stop();
}
let stopped;
try {
  component.net.isOnline();
} catch (error) {
  stopped = error.code;
}
if (stopped !== 'ERR_FIREFLY_APP_EXITED') {
  throw Error('Stopped network capability remained usable');
}
