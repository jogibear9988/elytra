/** The real utility parentPort adopts native endpoints before delivering this event. */
const parent = process.parentPort;
let filled = 0;
let port;
let received = 0;
let queued = false;
let large = false;

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

parent.on('message', ({ data, ports }) => {
  if (data.kind === 'fill') {
    check(ports.length === 0 && data.index === filled, 'Legacy fill FIFO');
    if (++filled === 16) {
      parent.postMessage({ kind: 'filled' });
    }
    return;
  }
  if (data.kind === 'exit') {
    check(queued && received === 32 && large, 'Port messages completed before exit');
    process.exit(0);
    return;
  }
  check(data.kind === 'attach' && !port && ports.length === 1, 'One transferred endpoint');
  port = ports[0];
  parent.postMessage({
    kind: 'identity',
    pid: process.pid,
    mite: typeof process.versions.mite === 'string',
    node: typeof process.versions.node === 'string'
  });
  let unsupported = false;
  try {
    parent.postMessage({}, [port]);
  } catch (error) {
    unsupported = error.code === 'ERR_FIREFLY_UNSUPPORTED_API';
  }
  check(unsupported, 'Child-to-parent native transfer remains explicit unsupported');

  port.on('message', ({ data }) => {
    if (data.kind === 'large') {
      check(queued && received === 32 && !large, 'Large logical data follows small messages once');
      check(
        data.bytes instanceof Uint8Array &&
          data.bytes.length === 512 * 1024 &&
          data.bytes.byteOffset === 8 &&
          data.bytes === data.again &&
          data.bytes.buffer === data.backing &&
          data.view.buffer === data.backing,
        'Large utility incoming graph aliases'
      );
      check(
        data.bytes.every((byte, index) => byte === ((index * 11 + 3) & 255)),
        'Large utility incoming snapshot'
      );
      large = true;
      port.postMessage({
        kind: 'large-echo',
        bytes: data.bytes,
        again: data.again,
        backing: data.backing,
        view: data.view
      });
      data.bytes.fill(211);
      port.close();
      return;
    }

    if (data.kind === 'queued-before-transfer') {
      check(!queued && received === 0, 'Pre-transfer queue retained');
      queued = true;
      port.postMessage({ kind: 'queued-echo' });
      return;
    }
    check(queued && data.kind === 'binary' && data.index === received, 'Port FIFO');
    check(
      data.bytes instanceof Uint8Array &&
        data.bytes.byteLength === 4096 &&
        data.bytes.every((value, offset) => value === ((offset * 13 + received) & 255)),
      'Transferred binary bytes'
    );
    port.postMessage({ kind: 'echo', index: received, bytes: data.bytes });
    data.bytes.fill(211);
    ++received;
  });
  // Deliberately let the native 16-message credit window fill before consuming.
  setTimeout(() => port.start(), 100);
});
