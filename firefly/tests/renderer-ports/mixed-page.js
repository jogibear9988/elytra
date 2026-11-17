/** A genuine principal-world DOM endpoint; no native testing API or injected bindings. */
(() => {
  const seen = new Set();

  window.addEventListener('message', (event) => {
    if (event.source !== window || event.data?.kind !== 'mixed-handoff') {
      return;
    }
    try {
      const phase = event.data.phase;
      if (seen.has(phase)) {
        throw Error('Duplicate mixed handoff');
      }
      seen.add(phase);
      if (
        typeof process !== 'undefined' ||
        event.ports.length !== 1 ||
        !(event.ports[0] instanceof MessagePort)
      ) {
        throw Error('Mixed handoff lacks real principal DOM port');
      }
      const port = event.ports[0];

      port.addEventListener('messageerror', () =>
        window.rendererPorts.report({ kind: 'failure', message: 'Mixed DOM messageerror' })
      );

      port.addEventListener('message', ({ data }) => {
        try {
          if (data.kind === 'ping') {
            port.postMessage({ kind: 'pong', nonce: data.nonce });
          } else if (data.kind === 'finish') {
            port.postMessage({ kind: 'tail', nonce: data.nonce });
            port.close();
          } else {
            throw Error('Unexpected mixed port data');
          }
        } catch (error) {
          window.rendererPorts.report({ kind: 'failure', message: error.message });
        }
      });

      port.start();
      window.rendererPorts.report({ kind: 'mixed-open', phase, epoch: location.search });
    } catch (error) {
      window.rendererPorts.report({ kind: 'failure', message: error.message });
    }
  });

  window.rendererPorts.report({ kind: 'mixed-ready', epoch: location.search });
})();
