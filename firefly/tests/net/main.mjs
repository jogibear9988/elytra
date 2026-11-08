/** Batched public networking acceptance; no BrowserWindow or renderer is created. */
import { app, net, utilityProcess } from 'firefly';
import exerciseNet from './transport.mjs';
// Keep the reference application's empty native process out of the desktop UI.
if (process.platform === 'darwin' && process.versions.electron && !process.versions.mite) {
  app.setActivationPolicy('prohibited');
}
const config = JSON.parse(process.env.ELYTRA_NET_TEST);

app.whenReady().then(async () => {
  const deadline = setTimeout(() => {
    console.error('Public net fixture timed out');
    app.exit(1);
  }, 15000);
  try {
    const result = await exerciseNet(net, { ...config, label: 'main' });
    console.log('PASS public main net transport ' + JSON.stringify(result));
    const child = utilityProcess.fork(new URL('./utility.mjs', import.meta.url).pathname, [], {
      env: { ...process.env, ELYTRA_NET_TEST: JSON.stringify(config) },
      stdio: 'pipe'
    });
    let error = '';
    child.stderr?.on('data', (value) => (error += value));
    child.stdout?.resume();
    let completed = false;
    const exited = new Promise((resolve, reject) => {
      child.on('error', reject);

      child.on('message', (value) => {
        if (value?.ok) {
          completed = true;
        } else {
          reject(Error(value?.error || 'Utility networking failed'));
        }
      });

      child.on('exit', resolve);
    });
    const status = await exited;
    if (status !== 0 || !completed || error) {
      throw Error('Utility net exit: ' + status + ' ' + error);
    }
    console.log('PASS public utility net transport');
    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    clearTimeout(deadline);
    console.error(error);
    app.exit(1);
  }
});
