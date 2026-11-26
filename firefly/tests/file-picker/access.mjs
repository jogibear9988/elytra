/** Browser-level acceptance for picked files, capability IPC, and real writable streams. */
import { app, BrowserWindow } from 'firefly';
import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';

const source = `<!doctype html><title>LOADING</title>
<button style="position:absolute;left:20px;top:20px;width:240px;height:50px">Pick</button>
<script type="module">
const check = (value, message) => { if (!value) throw Error(message); };
const operations = [
  async () => {
    const [handle] = await showOpenFilePicker({
      types: [{ description: 'Text', accept: { 'text/plain': ['.txt'] } }],
      excludeAcceptAllOption: true,
      startIn: 'documents',
    });
    check(handle instanceof FileSystemFileHandle && handle.kind === 'file', 'open handle type');
    check(handle.name === 'selected.txt' && await (await handle.getFile()).text() === 'original', 'selected file bytes');
    let denied = false;
    try { await handle.createWritable(); } catch (error) { denied = error.name === 'NotAllowedError'; }
    check(denied, 'read grant cannot write');
  },
  async () => {
    const directory = await showDirectoryPicker();
    check(directory instanceof FileSystemDirectoryHandle && directory.kind === 'directory', 'directory handle type');
    const child = await directory.getFileHandle('selected.txt');
    check(await (await child.getFile()).text() === 'original', 'directory relative lookup');
  },
  async () => {
    const handle = await showSaveFilePicker({ suggestedName: 'result.txt' });
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
    let cancelled = false;
    try { await showOpenFilePicker(); } catch (error) { cancelled = error.name === 'AbortError'; }
    check(cancelled, 'cancel rejects');
  },
];
let step = 0;
try {
  check(isSecureContext, 'secure fixture origin');
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

const server = createServer((request, response) => {
  response.setHeader('connection', 'close');
  if (request.url !== '/') {
    response.writeHead(404).end();
    return;
  }
  response.setHeader('content-type', 'text/html');
  response.end(source);
});
let window;
try {
  await app.whenReady();
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  window = new BrowserWindow({ width: 640, height: 400 });
  await window.loadURL(`http://127.0.0.1:${server.address().port}/`);
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
  console.log('PASS File System Access WebIDL, native picker IPC, streams and on-disk commit');
  window.destroy();
  await new Promise((resolve) => server.close(resolve));
  app.quit();
} catch (error) {
  console.error(error);
  window?.destroy();
  server.close();
  app.exit(1);
}
