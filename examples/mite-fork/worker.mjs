/** A real independent Mite/LibJS process with Node-shaped JSON IPC. */
process.on('message', ({ id, value }) => {
  process.send({ id, result: value * 2 });
});
// Peer disconnect closes IPC; no other referenced work remains, so this exits naturally.
