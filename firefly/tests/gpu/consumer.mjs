/** The pinned Code-OSS service receives the genuine app, never a fake Electron module. */
export default function exerciseConsumer(Service, app) {
  const check = (value, message) => {
    if (!value) {
      throw Error(message);
    }
  };

  const expected = app.getGPUFeatureStatus();
  const beforeUpdate = app.listenerCount('gpu-info-update');
  const beforeExit = app.listenerCount('child-process-gone');
  const service = new Service(app);
  const updates = [];
  const exits = [];
  const update = service.onDidUpdateFeatureStatus((status) => updates.push(status));
  const exit = service.onDidExitProcess((details) => exits.push(details));
  try {
    check(expected.gpu_compositing, 'Actual CPU-selected backend supplies known compositor policy');
    check(
      JSON.stringify(service.featureStatus) === JSON.stringify(expected),
      'Consumer reads genuine app snapshot'
    );
    expected.gpu_compositing = 'mutated external snapshot';
    check(
      service.featureStatus.gpu_compositing !== expected.gpu_compositing,
      'Consumer status owns its snapshot'
    );
    app.emit('gpu-info-update');
    check(updates.length === 0, 'Identical genuine status produces no consumer update');
    app.emit('child-process-gone', {}, { type: 'Utility', reason: 'clean-exit' });
    check(
      exits.length === 0 && service.featureStatus !== undefined,
      'Non-GPU synthetic exit ignored'
    );
    app.emit('child-process-gone', {}, { type: 'GPU', reason: 'crashed' });
    check(
      exits.length === 1 && exits[0].reason === 'crashed' && service.featureStatus === undefined,
      'Synthetic GPU exit retires consumer snapshot'
    );
    check(updates.length === 1 && updates[0] === undefined, 'Consumer publishes recovering state');
    app.emit('gpu-info-update');
    check(
      updates.length === 2 &&
        service.featureStatus.gpu_compositing === app.getGPUFeatureStatus().gpu_compositing,
      'Synthetic update refreshes from genuine native status'
    );
  } finally {
    update.dispose();
    exit.dispose();
    service.dispose();
  }
  check(
    app.listenerCount('gpu-info-update') === beforeUpdate &&
      app.listenerCount('child-process-gone') === beforeExit,
    'Consumer disposal removes both app listeners'
  );
  console.log(
    'PASS actual pinned Code-OSS GPUProcessMainService native-backed GPU status consumer'
  );
}
