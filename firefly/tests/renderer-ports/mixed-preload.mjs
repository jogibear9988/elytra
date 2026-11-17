/** Real ordinary-event pressure and canonical isolated-to-principal port handoff. */
import { ipcRenderer } from 'firefly';
const batches = new Map();

ipcRenderer.on('mixed:ordinary', (_event, batch, index) => {
  let state = batches.get(batch);
  if (!state) {
    state = { count: 0, ordered: true };
    batches.set(batch, state);
  }
  state.ordered &&= index === state.count;
  state.count++;
  if (state.count % 16 === 0) {
    ipcRenderer.send('ports:report', { kind: 'mixed-progress', batch, count: state.count });
  }
});

ipcRenderer.on('mixed:report', (_event, batch) => {
  const state = batches.get(batch);
  ipcRenderer.send('ports:report', {
    kind: 'mixed-ordinary',
    batch,
    count: state?.count ?? 0,
    ordered: state?.ordered ?? false
  });
});

ipcRenderer.on('mixed:handoff', (event, phase) => {
  window.postMessage({ kind: 'mixed-handoff', phase }, '*', event.ports);
});
