/** Browser-level acceptance for picked files, capability IPC, and real writable streams. */
import { app, BrowserWindow } from 'firefly';
import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';

app.setPath('userData', process.env.ELYTRA_TEST_PROFILE);

const workerSource = `
const check = (value, message) => { if (!value) throw Error(message); };
try {
  const directory = await navigator.storage.getDirectory();
  const file = await directory.getFileHandle('worker.txt', { create: true });
  const access = await file.createSyncAccessHandle();
  check(access instanceof FileSystemSyncAccessHandle, 'sync handle type');
  let lockError;
  try { await file.createWritable(); }
  catch (error) { lockError = error.name; }
  check(lockError === 'NoModificationAllowedError', 'unexpected state for a competing stream: ' + lockError);
  const data = new Uint8Array([120, 119, 111, 114, 107, 101, 114]);
  check(access.write(data.subarray(1), { at: 0 }) === 6, 'sync write respects view offset');
  check(access.getSize() === 6, 'sync size');
  const read = new Uint8Array(8);
  check(access.read(read.subarray(1), { at: 0 }) === 6, 'sync short read');
  check(String.fromCharCode(...read.subarray(1, 7)) === 'worker', 'sync read bytes');
  check(read[0] === 0 && read[7] === 0, 'sync read respects view boundaries');
  access.truncate(4);
  access.flush();
  access.close();
  access.close();
  let closed = false;
  try { access.getSize(); } catch (error) { closed = error.name === 'InvalidStateError'; }
  check(closed, 'closed sync handle rejects operations');
  const readonly = await file.createSyncAccessHandle({ mode: 'read-only' });
  let denied = false;
  try { readonly.write(data); } catch (error) { denied = error.name === 'NoModificationAllowedError'; }
  check(denied, 'read-only sync handle rejects writes');
  readonly.close();
  check(await (await file.getFile()).text() === 'work', 'async read sees sync commit');
  globalThis.retainedAccess = await file.createSyncAccessHandle();
  postMessage({ ok: true });
} catch (error) {
  postMessage({ error: error.name + ': ' + error.message });
}
`;

const cloneWorker = `
self.onmessage = async ({ data }) => {
  try {
    const file = await data.getFileHandle('private.txt');
    const handle = await file.createSyncAccessHandle();
    try {
      const buffer = new Uint8Array(7);
      handle.read(buffer);
      postMessage({ contents: String.fromCharCode(...buffer), file });
    } finally {
      handle.close();
    }
  } catch (error) {
    postMessage({ error: error.name + ': ' + error.message });
  }
};
`;

const storageCode = `
function openDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open('file-system-handle-fixture', 1);
    request.onupgradeneeded = () => request.result.createObjectStore('handles');
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function persistHandle(handle) {
  const database = await openDatabase();
  try {
    await new Promise((resolve, reject) => {
      const transaction = database.transaction('handles', 'readwrite');
      transaction.objectStore('handles').put(handle, 'private');
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    database.close();
  }
}

async function restoreHandle() {
  const database = await openDatabase();
  try {
    return await new Promise((resolve, reject) => {
      const request = database.transaction('handles').objectStore('handles').get('private');
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  } finally {
    database.close();
  }
}
`;

const source = `<!doctype html><title>LOADING</title>
<button style="position:absolute;left:20px;top:20px;width:240px;height:50px">Pick</button>
<script type="module">
${storageCode}
const check = (value, message) => { if (!value) throw Error(message); };
let selected;
let selectedDirectory;
const operations = [
  async () => {
    const [handle] = await showOpenFilePicker({
      types: [{ description: 'Text', accept: { 'text/plain': ['.txt'] } }],
      excludeAcceptAllOption: true,
      startIn: 'documents',
    });
    check(handle instanceof FileSystemFileHandle && handle.kind === 'file', 'open handle type');
    const file = await handle.getFile();
    check(handle.name === 'selected.txt' && await file.text() === 'original', 'selected file bytes');
    check(file.lastModified === 1700000000000 && file.type === 'text/plain', 'selected file metadata');
    let denied = false;
    try { await handle.createWritable(); } catch (error) { denied = error.name === 'NotAllowedError'; }
    check(denied, 'read grant cannot write');
    check(await handle.queryPermission() === 'granted', 'selected read permission');
    check(await handle.queryPermission({ mode: 'readwrite' }) === 'prompt', 'read grant can request write');
    selected = handle;
  },
  async () => {
    const directory = await showDirectoryPicker({ mode: 'readwrite', id: 'fixtures', startIn: selected });
    check(directory instanceof FileSystemDirectoryHandle && directory.kind === 'directory', 'directory handle type');
    const child = await directory.getFileHandle('selected.txt');
    check(await (await child.getFile()).text() === 'original', 'directory relative lookup');
    check(await child.isSameEntry(selected), 'independently acquired handles identify one file');
    check((await directory.resolve(selected)).join('/') === 'selected.txt', 'resolve independent child');
    check((await directory.resolve(directory)).length === 0, 'resolve self');
    check(await directory.queryPermission({ mode: 'readwrite' }) === 'granted', 'writable folder permission');
    const nested = await directory.getDirectoryHandle('new-folder', { create: true });
    const created = await nested.getFileHandle('new.txt', { create: true });
    check((await directory.resolve(created)).join('/') === 'new-folder/new.txt', 'resolve nested file');
    check(await nested.resolve(selected) === null, 'resolve unrelated file');
    const stream = await created.createWritable();
    await stream.write('child bytes');
    await stream.close();
    check(await (await created.getFile()).text() === 'child bytes', 'child inherits write grant');
    let count = 0;
    for await (const [name, entry] of directory) {
      check(name === entry.name, 'directory iterator pairs');
      count += 1;
    }
    check(count === 2, 'directory iterator reaches end');
    for await (const name of directory.keys()) {
      check(typeof name === 'string', 'directory keys');
      break;
    }
    for await (const entry of directory.values()) {
      check(entry instanceof FileSystemHandle, 'directory values');
      break;
    }
    let refusedNonEmpty = false;
    try { await directory.removeEntry('new-folder'); }
    catch (error) { refusedNonEmpty = error.name === 'InvalidModificationError'; }
    check(refusedNonEmpty, 'non-empty directory requires recursive removal');
    await directory.removeEntry('new-folder', { recursive: true });
    await directory.removeEntry('new-folder');
    let missing = false;
    try { await directory.getDirectoryHandle('new-folder'); }
    catch (error) { missing = error.name === 'NotFoundError'; }
    check(missing, 'recursive removal');
    selectedDirectory = directory;
  },
  async () => {
    const handle = await showSaveFilePicker({ suggestedName: 'result.txt', startIn: selectedDirectory, id: 'fixtures' });
    check(handle.name === 'result.txt', 'save name');
    let stream = await handle.createWritable();
    check(stream instanceof FileSystemWritableFileStream && stream instanceof WritableStream, 'writable stream type');
    await stream.write('hello world');
    await stream.seek(6);
    await stream.write(new Blob(['Elytra']));
    await stream.truncate(9);
    check(await (await handle.getFile()).text() === '', 'atomic close boundary');
    await stream.close();
    check(await (await handle.getFile()).text() === 'hello Ely', 'close commits bytes');
    stream = await handle.createWritable({ keepExistingData: true });
    const writer = stream.getWriter();
    await writer.write({ type: 'write', position: 8, data: new Uint8Array([121, 116, 114, 97]) });
    await writer.close();
    stream = await handle.createWritable();
    await new ReadableStream({
      start(controller) {
        controller.enqueue('hello Elytra');
        controller.close();
      },
    }).pipeTo(stream);
    stream = await handle.createWritable();
    await stream.write('discard');
    await stream.abort();
    check(await (await handle.getFile()).text() === 'hello Elytra', 'abort retains committed bytes');
  },
  async () => {
    const permission = await selected.requestPermission({ mode: 'readwrite' });
    check(permission === 'granted', 'permission prompt granted');
    const stream = await selected.createWritable();
    await stream.write('modified with permission');
    await stream.close();
  },
  async () => {
    let cancelled = false;
    try { await showOpenFilePicker(); } catch (error) { cancelled = error.name === 'AbortError'; }
    check(cancelled, 'cancel rejects');
  },
];
let step = 0;
try {
  check(isSecureContext, 'secure fixture origin');
  const privateRoot = await navigator.storage.getDirectory();
  check(privateRoot instanceof FileSystemDirectoryHandle && privateRoot.name === '', 'origin-private root');
  const privateFile = await privateRoot.getFileHandle('private.txt', { create: true });
  const privateStream = await privateFile.createWritable();
  await privateStream.write('private contents');
  await privateStream.close();
  let quota = false;
  const large = await privateFile.createWritable();
  try { await large.truncate(65 * 1024 * 1024); }
  catch (error) { quota = error.name === 'QuotaExceededError'; }
  check(quota, 'origin-private quota');
  const recheck = await privateFile.createWritable({ keepExistingData: true });
  await recheck.close();
  check(await (await privateFile.getFile()).text() === 'private contents', 'quota failure retains file');

  const cloned = structuredClone({ file: privateFile, repeat: privateFile, root: privateRoot });
  check(cloned.file === cloned.repeat, 'clone retains reference identity');
  check(cloned.file instanceof FileSystemFileHandle, 'clone retains file prototype');
  check(cloned.root instanceof FileSystemDirectoryHandle, 'clone retains directory prototype');
  check(cloned.file !== privateFile && await cloned.file.isSameEntry(privateFile), 'clone points to original file');
  check((await cloned.root.resolve(cloned.file)).join('/') === 'private.txt', 'clone resolves within cloned directory');
  check(await (await cloned.file.getFile()).text() === 'private contents', 'clone reads existing file');
  const fresh = structuredClone(privateFile);
  const [snapshot, permission] = await Promise.all([fresh.getFile(), fresh.queryPermission()]);
  check(await snapshot.text() === 'private contents' && permission === 'granted', 'concurrent first use of cloned handle');
  const removed = structuredClone(await privateRoot.getFileHandle('removed.txt', { create: true }));
  await privateRoot.removeEntry('removed.txt');
  let missingFromClone = false;
  try { await removed.getFile(); } catch (error) { missingFromClone = error.name === 'NotFoundError'; }
  check(missingFromClone, 'clone reports a removed private entry');
  await persistHandle(cloned.file);

  const clone = new Worker('/clone-worker.js', { type: 'module' });
  const clonedInWorker = await new Promise((resolve, reject) => {
    clone.onmessage = ({ data }) => resolve(data);
    clone.onmessageerror = () => reject(Error('cloned worker returned an unreadable handle'));
    clone.onerror = (event) => reject(Error(event.message || 'clone worker failed'));
    clone.postMessage(cloned.root);
  }).finally(() => clone.terminate());
  check(clonedInWorker.contents === 'private', clonedInWorker.error || 'worker reads cloned handle');
  check(await clonedInWorker.file.isSameEntry(privateFile), 'worker returns a same-origin file handle');

  const targetOrigin = new URL(location.href).searchParams.get('clone-origin');
  const frame = document.createElement('iframe');
  frame.hidden = true;
  frame.src = targetOrigin + '/clone-boundary';
  const loaded = new Promise((resolve) => frame.addEventListener('load', resolve, { once: true }));
  document.body.appendChild(frame);
  await loaded;
  const rejected = new Promise((resolve) => {
    const receive = (event) => {
      if (event.origin === targetOrigin && event.source === frame.contentWindow) {
        window.removeEventListener('message', receive);
        resolve(event.data);
      }
    };
    window.addEventListener('message', receive);
  });
  frame.contentWindow.postMessage(cloned.file, targetOrigin);
  check(await rejected === 'clone-blocked', 'different origin cannot deserialize a private file handle');
  frame.remove();



  check(typeof FileSystemFileHandle.prototype.createSyncAccessHandle === 'undefined', 'sync API hidden in Window');
  const worker = new Worker('/worker.js', { type: 'module' });
  const outcome = await new Promise((resolve, reject) => {
    worker.onmessage = (event) => resolve(event.data);
    worker.onerror = (event) => reject(Error(event.message || 'filesystem worker failed'));
  }).finally(() => worker.terminate());
  check(outcome.ok, outcome.error);
  const fromWorker = await privateRoot.getFileHandle('worker.txt');
  check(await (await fromWorker.getFile()).text() === 'work', 'worker and Window share origin storage');

  let afterTermination;
  for (let attempt = 0; attempt < 50; ++attempt) {
    try {
      afterTermination = await fromWorker.createWritable({ keepExistingData: true });
      break;
    } catch (error) {
      if (error.name !== 'NoModificationAllowedError') throw error;
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
  }
  check(afterTermination, 'terminating the worker releases its retained sync lock');
  await afterTermination.abort();

  const chunks = [];
  const plain = new WritableStream({ write: (chunk) => chunks.push(chunk) });
  const writer = new WritableStreamDefaultWriter(plain);
  await writer.write('base stream');
  await writer.close();
  check(chunks.join() === 'base stream', 'ordinary WritableStream still writes');

  for (const picker of [showOpenFilePicker, showDirectoryPicker, showSaveFilePicker]) {
    let rejected = false;
    try { await picker(); } catch (error) { rejected = error.name === 'SecurityError'; }
    check(rejected, 'no activation rejects');
  }
  const button = document.querySelector('button');
  button.onclick = async () => {
    button.disabled = true;
    try {
      await operations[step++]();
      document.title = step === operations.length ? 'PASS_FILE_ACCESS_IPC' : 'PICKER_READY_' + step;
      button.disabled = false;
    } catch (error) {
      document.title = 'FAIL: ' + error.name + ': ' + error.message;
    }
  };
  document.title = 'PICKER_READY_0';
} catch (error) {
  document.title = 'FAIL: ' + error.name + ': ' + error.message;
}
</script>`;

const reloaded = `<!doctype html><title>CHECKING</title><script type="module">
${storageCode}
try {
  const directory = await navigator.storage.getDirectory();
  const file = await directory.getFileHandle('private.txt');
  const stored = await restoreHandle();
  document.title = stored instanceof FileSystemFileHandle && await stored.isSameEntry(file)
    && await (await stored.getFile()).text() === 'private contents' ? 'PASS_RELOAD' : 'FAIL:stored handle';
} catch (error) {
  document.title = error.name === 'NotFoundError' ? 'PASS_ISOLATED' : 'FAIL:' + error.name;
}
</script>`;

const otherOrigin = createServer((request, response) => {
  response.setHeader('content-type', 'text/html');
  response.setHeader('connection', 'close');
  if (request.url === '/clone-boundary') {
    response.end(`<!doctype html><script>
      onmessage = () => parent.postMessage('clone-escaped', '*');
      onmessageerror = () => parent.postMessage('clone-blocked', '*');
    </script>`);
    return;
  }
  response.end(reloaded);
});

const server = createServer((request, response) => {
  response.setHeader('connection', 'close');
  if (request.url === '/worker.js' || request.url === '/clone-worker.js') {
    response.setHeader('content-type', 'text/javascript');
    response.end(request.url === '/worker.js' ? workerSource : cloneWorker);
    return;
  }
  if (request.url?.split('?')[0] !== '/' && request.url !== '/reload') {
    response.writeHead(404).end();
    return;
  }
  response.setHeader('content-type', 'text/html');
  response.end(request.url === '/reload' ? reloaded : source);
});
let window;
try {
  await app.whenReady();
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  await new Promise((resolve) => otherOrigin.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${otherOrigin.address().port}`;
  window = new BrowserWindow({ width: 640, height: 400 });
  await window.loadURL(`http://127.0.0.1:${server.address().port}/?clone-origin=${origin}`);
  let title;
  for (let i = 0; i < 1000; i++) {
    title = window.webContents.getTitle();
    if (title === 'PASS_FILE_ACCESS_IPC' || title.startsWith('FAIL:')) {
      break;
    }
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  if (title !== 'PASS_FILE_ACCESS_IPC') {
    throw Error(`Unexpected title ${title}`);
  }
  if (readFileSync(process.env.ELYTRA_TEST_SAVE_FILE, 'utf8') !== 'hello Elytra') {
    throw Error('Saved bytes on disk did not match renderer readback');
  }
  if (readFileSync(process.env.ELYTRA_TEST_OPEN_FILE, 'utf8') !== 'modified with permission') {
    throw Error('Permission escalation did not authorize the selected file write');
  }

  async function expectStorageTitle(url, expected) {
    await window.loadURL(url);
    for (let i = 0; i < 500; ++i) {
      const observed = window.webContents.getTitle();
      if (observed === expected) {
        return;
      }
      if (observed.startsWith('FAIL:')) {
        throw Error(observed);
      }
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    throw Error(`Storage check expected ${expected}, got ${window.webContents.getTitle()}`);
  }

  await expectStorageTitle(`http://127.0.0.1:${server.address().port}/reload`, 'PASS_RELOAD');
  await expectStorageTitle(`${origin}/`, 'PASS_ISOLATED');
  await new Promise((resolve) => otherOrigin.close(resolve));
  console.log('PASS File System Access WebIDL, native picker IPC, streams and on-disk commit');
  window.destroy();
  await new Promise((resolve) => server.close(resolve));
  app.quit();
} catch (error) {
  console.error(error);
  window?.destroy();
  server.close();
  otherOrigin.close();
  app.exit(1);
}
