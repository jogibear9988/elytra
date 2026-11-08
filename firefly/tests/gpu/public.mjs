/** Actual selected compositor policy, with no windows or invented GPU device capabilities. */
import { app } from 'firefly';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const unknown = process.argv.includes('--unknown');
let updates = 0;

app.on('gpu-info-update', () => {
  updates++;
});

if (unknown) {
  check(JSON.stringify(app.getGPUFeatureStatus()) === '{}', 'Pre-ready default policy unknown');
} else {
  app.disableHardwareAcceleration();
  app.disableHardwareAcceleration();
}

app.whenReady().then(async () => {
  try {
    const first = app.getGPUFeatureStatus();
    check(first && typeof first === 'object' && !Array.isArray(first), 'GPU status snapshot');
    if (unknown) {
      check(
        JSON.stringify(first) === '{}' && updates === 0,
        'Default readiness cannot invent GPU status or update'
      );
      console.log(
        'PASS public default GPU status remains unknown before and after ready; no invented GPU update'
      );
      app.exit(0);
      return;
    }
    check(typeof first.gpu_compositing === 'string', 'Selected CPU compositing status known');
    check(
      !Object.values(first).some((value) => value === 'enabled'),
      'CPU policy cannot advertise enabled GPU features'
    );
    const second = app.getGPUFeatureStatus();
    check(
      second !== first && JSON.stringify(first) === JSON.stringify(second),
      'Independent stable status snapshots'
    );
    first.gpu_compositing = 'forged';
    check(
      app.getGPUFeatureStatus().gpu_compositing === second.gpu_compositing,
      'Returned mutation cannot alter native policy'
    );
    const deadline = Date.now() + 1000;
    while (updates === 0 && Date.now() < deadline) {
      await new Promise((resolve) => setTimeout(resolve, 5));
    }
    check(updates === 1, 'One genuine native policy information update follows readiness');
    let lateFailure;
    try {
      app.disableHardwareAcceleration();
    } catch (failure) {
      lateFailure = failure;
    }
    check(lateFailure, 'Late acceleration policy change rejects explicitly');
    console.log(
      'PASS public native-backed CPU compositor policy snapshots and lifecycle; no GPU device/window claim'
    );
    app.exit(0);
  } catch (failure) {
    console.error(failure);
    app.exit(1);
  }
});
