/** Author-side dedicated module Worker only; no preload/native diagnostic exports. */
const tag = new URL(location.href).searchParams.get('tag') ?? 'original';
// Custom-protocol modules share the creator's native origin. Loopback is only
// the independent HTTP control/payload endpoint, not the protocol authority.
const control = new URL(location.href).searchParams.get('control');

const report = (value) =>
  fetch(control + '/report?value=' + encodeURIComponent(JSON.stringify({ tag, ...value }))).catch(
    () => {}
  );

const source = `
let nested;
postMessage({phase:'worker-ready'});
onmessage = async ({data}) => {
  try {
    if (data.operation === 'basic') {
      const module = await import(data.module);
      const response = await fetch(data.allowed);
      const edited = response.headers.get('x-worker-edited');
      const text = await response.text();
      let deniedHTTP = false, deniedModule = false;
      try { await fetch(data.denied); } catch { deniedHTTP = true; }
      try { await import(data.deniedModule); } catch { deniedModule = true; }
      postMessage({phase:'basic', token:module.fixtureToken, text, edited, deniedHTTP, deniedModule,
        bytes:new Uint8Array([0,128,255])});
    } else if (data.operation === 'nested-start') {
      const url = URL.createObjectURL(new Blob([data.source], {type:'text/javascript'}));
      nested = new Worker(url, {type:'module'});
      nested.onmessage = event => postMessage({...event.data, phase:'nested-'+event.data.phase});
      nested.onerror = () => postMessage({phase:'nested-worker-error-event'});
    } else if (data.operation === 'nested-message') {
      nested.postMessage(data.message);
    } else if (data.operation === 'hold') {
      postMessage({phase:'hold-started', target:data.target});
      try { await fetch(data.target); postMessage({phase:'stale-result'}); }
      catch { postMessage({phase:'hold-rejected'}); }
    } else if (data.operation === 'barrier') {
      const text = await (await fetch(data.target)).text();
      postMessage({phase:'barrier', text, target:data.target});
    }
  } catch(error) { postMessage({phase:'worker-error', name:error.name}); }
};`;
let worker;
let blob;

function start() {
  if (worker) {
    worker.terminate();
  }
  if (blob) {
    URL.revokeObjectURL(blob);
  }
  blob = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
  worker = new Worker(blob, { type: 'module' });

  worker.onmessage = (event) => {
    const value = event.data;
    if (value.bytes) {
      value.bytes = Array.from(value.bytes);
    }
    void report(value);
  };

  worker.onerror = () => {
    void report({ phase: 'worker-error-event' });
  };
}

await report({ phase: 'document-ready' });
let nonce = 0;
while (true) {
  try {
    const command = await (await fetch(control + '/commands')).json();
    if (command && command.tag === tag && command.nonce > nonce) {
      nonce = command.nonce;
      if (command.operation === 'start') {
        start();
      } else if (command.operation === 'terminate') {
        worker.terminate();
        worker = null;
        if (blob) {
          URL.revokeObjectURL(blob);
        }
        blob = null;
        await report({ phase: 'terminated' });
      } else if (command.operation === 'nested-start') {
        worker.postMessage({ ...command, source });
      } else {
        worker.postMessage(command);
      }
    }
  } catch {
    /* A real document replacement cancels the old control request. */
  }
  await new Promise((resolve) => setTimeout(resolve, 10));
}
