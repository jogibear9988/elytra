/** Real native writes remain rooted after external FileHandle/stream references disappear. */
import fs from 'node:fs';
import { open } from 'node:fs/promises';
import { check } from './support.mjs';

check(
  typeof collectHostGarbage === 'function' && typeof collectHostGarbageAtIdle === 'function',
  'Requires actual embedded LibJS collectors'
);
const originalWrite = fs.write;
const phasesByFd = new Map();

const nextTurn = () => new Promise((resolve) => setImmediate(resolve));

/** This callback has no closure over a FileHandle, stream, write callback or buffer. */
function collectSubmitted(phase) {
  check(phase.pending > 0, 'Actual native write still pending before its completion callback');
  collectHostGarbage();
  ++phase.collections;
  check(phase.handle.deref() !== undefined, 'Native write keeps its FileHandle alive');
  check(phase.stream.deref() !== undefined, 'Native write keeps its Writable alive');
}

// Observe the public submission boundary without changing bytes, offsets, errors,
// completion scheduling or the actual native operation. A nextTick collection runs
// after this stack returns and before libuv can dispatch the write completion.
fs.write = function (fd, ...args) {
  const phase = phasesByFd.get(fd);
  if (!phase) {
    return originalWrite.call(fs, fd, ...args);
  }
  const callback = args[args.length - 1];
  ++phase.pending;
  ++phase.submissions;

  args[args.length - 1] = function (...values) {
    --phase.pending;
    ++phase.completions;
    return callback(...values);
  };

  let result;
  try {
    result = originalWrite.call(fs, fd, ...args);
  } catch (error) {
    --phase.pending;
    throw error;
  }
  process.nextTick(collectSubmitted, phase);
  return result;
};

function recordWrite(phase, error) {
  check(!error, 'Accepted write completes successfully after collection');
  ++phase.writeCallbacks;
}

function recordEnd(phase, error) {
  check(!error, 'End callback succeeds after collection');
  ++phase.endCallbacks;
}

function recordFinish(phase) {
  ++phase.finishes;
}

function recordClose(phase) {
  ++phase.closes;
  phase.resolve();
}

function failPhase(phase, error) {
  phase.reject(error);
}

function recordHandleClose(phase) {
  ++phase.handleCloses;
}

/** Completed setup frames release all strong application references to both owners. */
function startStream(handle, phase) {
  const stream = handle.createWriteStream({ highWaterMark: 4096 });
  phase.fd = handle.fd;
  phase.handle = new WeakRef(handle);
  phase.stream = new WeakRef(stream);
  phasesByFd.set(phase.fd, phase);
  stream.on('finish', recordFinish.bind(undefined, phase));
  stream.on('close', recordClose.bind(undefined, phase));
  stream.on('error', failPhase.bind(undefined, phase));
  handle.on('close', recordHandleClose.bind(undefined, phase));
  stream.write(Buffer.alloc(8 * 1024 * 1024, phase.fill), recordWrite.bind(undefined, phase));
  if (phase.manual) {
    stream.write(Buffer.from([0, 255, phase.fill]), recordWrite.bind(undefined, phase));
    // The stream lease must preserve queued data while this close is pending.
    handle.close().then(phase.resolveManual, phase.reject);
  } else {
    stream.end(Buffer.from([0, 255, phase.fill]), recordEnd.bind(undefined, phase));
  }
}

function phase(manual) {
  const state = {
    manual,
    fill: manual ? 0x73 : 0x2a,
    pending: 0,
    submissions: 0,
    completions: 0,
    collections: 0,
    writeCallbacks: 0,
    endCallbacks: 0,
    finishes: 0,
    closes: 0,
    handleCloses: 0
  };
  state.closed = new Promise((resolve, reject) => {
    state.resolve = resolve;
    state.reject = reject;
  });
  state.manualClosed = new Promise((resolve) => {
    state.resolveManual = resolve;
  });
  return state;
}

try {
  for (const manual of [false, true]) {
    const state = phase(manual);
    const filename = manual ? 'gc-manual-close.bin' : 'gc-end.bin';
    // The fulfilled chain carries undefined, never the FileHandle or stream.
    await open(filename, 'w+').then((handle) => startStream(handle, state));
    await state.closed;
    if (manual) {
      await state.manualClosed;
    }
    phasesByFd.delete(state.fd);
    check(
      state.pending === 0 && state.submissions === state.completions,
      'Every native write retired'
    );
    check(state.collections > 0, 'Collected during actual pending native writes');
    check(
      state.finishes === 1 && state.closes === 1 && state.handleCloses === 1,
      'Owners close exactly once'
    );
    check(
      state.writeCallbacks === (manual ? 2 : 1),
      'Every queued write callback survives collection'
    );
    check(state.endCallbacks === (manual ? 0 : 1), 'End callback survives collection');
    const bytes = fs.readFileSync(filename);
    check(bytes.byteLength === 8 * 1024 * 1024 + 3, 'All accepted bytes reached real file');
    // The Python runner independently compares every byte; keep this JS check
    // bounded so an interpreter's loop speed does not dominate the GC fixture.
    check(
      bytes[0] === state.fill && bytes[bytes.length - 4] === state.fill,
      'Native payload boundaries remain intact'
    );
    check(
      bytes[bytes.length - 3] === 0 &&
        bytes[bytes.length - 2] === 255 &&
        bytes[bytes.length - 1] === state.fill,
      'Queued binary tail preserved'
    );
    let closed = false;
    try {
      fs.fstatSync(state.fd);
    } catch (error) {
      closed = error.code === 'EBADF';
    }
    check(closed, 'Original descriptor has actually closed');
    const sentinel = fs.openSync('gc-sentinel.bin', 'w+');
    try {
      for (let turn = 0; turn < 8; ++turn) {
        await nextTurn();
        await collectHostGarbageAtIdle();
      }
      check(
        state.handle.deref() === undefined && state.stream.deref() === undefined,
        'Completed owners are reclaimable'
      );
      fs.writeSync(sentinel, 'still-owned');
      check(fs.fstatSync(sentinel).size === 11, 'Collection never closes another descriptor');
    } finally {
      fs.closeSync(sentinel);
    }
    console.log(
      'PASS real native FileHandle WriteStream GC lifetime',
      manual ? 'manual-close' : 'end'
    );
  }
} finally {
  fs.write = originalWrite;
}
