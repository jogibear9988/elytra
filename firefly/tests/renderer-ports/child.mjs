/** Real utility endpoint becomes the renderer's peer through the main relay. */
const parent = process.parentPort;
let port;
let count = 0;

parent.on('message', ({ data, ports }) => {
  if (data === 'exit') {
    process.exit(0);
    return;
  }
  if (data !== 'attach' || ports.length !== 1 || port) {
    throw Error('One real utility endpoint');
  }
  port = ports[0];

  port.on('message', ({ data: value }) => {
    if (value.kind === 'queued-echo') {
      if (count !== 0 || value.phase !== 'relay') {
        throw Error('Relay pre-transfer FIFO');
      }
      count++;
      return;
    }
    if (
      value.kind !== 'echo' ||
      value.phase !== 'relay' ||
      value.index !== count - 1 ||
      !(value.bytes instanceof Uint8Array) ||
      value.bytes.byteLength !== 4096 ||
      !value.bytes.every((byte, offset) => byte === ((offset * 13 + value.index) & 255))
    ) {
      throw Error('Genuine utility/renderer binary FIFO snapshot');
    }
    count++;
  });

  port.once('close', () => {
    if (count !== 33) {
      throw Error('All 33 renderer echoes precede utility peer close');
    }
    parent.postMessage({
      kind: 'relay-complete',
      count,
      pid: process.pid,
      mite: !!process.versions.mite
    });
  });

  port.start();
  port.postMessage({ kind: 'queued' });
  for (let index = 0; index < 32; index++) {
    const bytes = new Uint8Array(4096);
    for (let offset = 0; offset < bytes.length; offset++) {
      bytes[offset] = (offset * 13 + index) & 255;
    }
    port.postMessage({ kind: 'binary', index, bytes });
    bytes.fill(77);
  }
  parent.postMessage({ kind: 'attached' });
});
