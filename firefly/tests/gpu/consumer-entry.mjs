/** Genuine app status and unchanged consumer; synthetic events test consumer logic, not OS crashes. */
import { app } from 'firefly';
import exerciseConsumer from './consumer.mjs';

app.disableHardwareAcceleration();

app.whenReady().then(async () => {
  const timeout = setTimeout(() => {
    console.error('GPU consumer timed out');
    app.exit(1);
  }, 10000);
  try {
    const { GPUProcessMainService } = await import(process.env.ELYTRA_GPU_CONSUMER);
    exerciseConsumer(GPUProcessMainService, app);
    clearTimeout(timeout);
    app.exit(0);
  } catch (failure) {
    clearTimeout(timeout);
    console.error(failure);
    app.exit(1);
  }
});
