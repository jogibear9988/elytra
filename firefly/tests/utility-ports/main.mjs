/** Real no-window utility process: native transfer, bounded pressure, FIFO and close ownership. */
import { app, MessageChannelMain, utilityProcess } from 'firefly';
import path from 'node:path';

app.setPath('userData', path.join(import.meta.dirname, 'config'));
let child;
let ended = false;
const timer = setTimeout(() => fail(Error('Utility MessagePort acceptance timed out')), 15000);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fail(error) {
  if (ended) {
    return;
  }
  ended = true;
  clearTimeout(timer);
  console.error(error.stack || error.message);
  child?.kill();
  app.exit(1);
}

async function until(predicate) {
  for (let attempt = 0; attempt < 1000; attempt++) {
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Utility MessagePort condition did not settle');
}

app
  .whenReady()
  .then(async () => {
    child = utilityProcess.fork(path.join(import.meta.dirname, 'child.mjs'), [], { stdio: 'pipe' });
    child.on('error', fail);
    const notices = [];

    child.on('message', (value) => notices.push(value));

    let exitCode;

    child.once('exit', (code) => {
      exitCode = code;
    });

    let stderr = '';
    child.stderr?.on('data', (data) => {
      stderr += data.toString();
    });

    // Native writes complete only after this synchronous turn. Fill the actual
    // 16-write limit, then prove failed transfer admission keeps local ownership.
    for (let index = 0; index < 16; index++) {
      child.postMessage({ kind: 'fill', index });
    }
    const failed = new MessageChannelMain();
    let refused = false;
    try {
      child.postMessage({ kind: 'must-not-arrive' }, [failed.port2]);
    } catch (error) {
      refused = error.code === 'ERR_FIREFLY_UTILITY_CHANNEL';
    }
    check(refused, 'Native full-write queue must reject transfer');
    let stillOwned = false;

    failed.port2.once('message', (event) => {
      stillOwned = event.data === 'still-owned';
    });

    failed.port2.start();
    failed.port1.postMessage('still-owned');
    await until(() => stillOwned && notices.some((value) => value.kind === 'filled'));
    failed.port1.close();
    failed.port2.close();

    const channel = new MessageChannelMain();
    const order = [];
    let closed = false;

    channel.port1.on('message', (event) => {
      const data = event.data;
      if (data.kind === 'queued-echo') {
        check(order.length === 0, 'Queued pre-transfer message arrives first');
        order.push('queued');
        return;
      }
      check(data.kind === 'echo' && data.index === order.length - 1, 'Binary FIFO index');
      check(data.bytes instanceof Uint8Array && data.bytes.byteLength === 4096, 'Binary view');
      check(
        data.bytes.every((value, offset) => value === ((offset * 13 + data.index) & 255)),
        'Independent binary snapshot'
      );
      order.push(data.index);
    });

    channel.port1.once('close', () => {
      check(order.length === 33, 'All accepted messages precede peer close');
      closed = true;
    });

    channel.port1.start();
    channel.port1.postMessage({ kind: 'queued-before-transfer' });
    child.postMessage({ kind: 'attach' }, [channel.port2]);
    // Detached wrappers must neither send nor close the new owner.
    channel.port2.postMessage({ kind: 'detached-must-not-arrive' });
    channel.port2.close();
    for (let index = 0; index < 32; index++) {
      const bytes = new Uint8Array(4096);
      for (let offset = 0; offset < bytes.length; offset++) {
        bytes[offset] = (offset * 13 + index) & 255;
      }
      channel.port1.postMessage({ kind: 'binary', index, bytes });
      bytes.fill(77);
    }
    await until(() => closed);
    const identity = notices.find((value) => value.kind === 'identity');
    check(
      identity &&
        identity.mite &&
        !identity.node &&
        identity.pid === child.pid &&
        identity.pid !== process.pid,
      'Genuine separate Mite utility child'
    );
    child.postMessage({ kind: 'exit' });
    await until(() => exitCode !== undefined);
    check(exitCode === 0 && child.pid === undefined, 'Child exit and PID retirement');
    check(!stderr, 'Utility stderr: ' + stderr);
    check(notices.filter((value) => value.kind === 'filled').length === 1, 'No duplicate delivery');
    ended = true;
    clearTimeout(timer);
    console.log(
      'PASS actual utility MessagePorts: failed native admission preserves ownership, ' +
        'queued transfer, 32 binary FIFO snapshots under delayed start, peer close and child exit'
    );
    app.quit();
  })
  .catch(fail);
