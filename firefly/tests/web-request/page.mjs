/** Real renderer requests. Each origin authors its own fetch and image operations. */
const params = new URL(location.href).searchParams;
const tag = params.get('tag') ?? 'main';
const control = params.get('control') ?? location.origin;

const report = (value) =>
  fetch(
    control +
      '/report?value=' +
      encodeURIComponent(JSON.stringify({ tag, url: location.href, ...value }))
  ).catch(() => {});

void report({ kind: 'ready' });
if (params.has('cross')) {
  const child = document.createElement('iframe');
  child.src = params.get('cross') + '/page.html?tag=child&control=' + encodeURIComponent(control);
  document.body.append(child);
}
let last = 0;
while (true) {
  let command;
  try {
    command = await (await fetch(control + '/commands', { cache: 'no-store' })).json();
  } catch {
    await new Promise((resolve) => setTimeout(resolve, 20));
    continue;
  }
  if (command && command.tag === tag && command.nonce > last) {
    last = command.nonce;
    let result;
    try {
      if (command.kind === 'socket') {
        const socket = new WebSocket(command.url);
        await new Promise((resolve, reject) => {
          socket.onopen = () => {
            socket.close();
            resolve();
          };

          socket.onerror = () => reject(Error('WebSocket failed'));
        });
        result = { ok: true };
      } else if (command.kind === 'image') {
        const image = document.createElement('img');
        await new Promise((resolve, reject) => {
          image.onload = resolve;

          image.onerror = () => reject(Error('Image failed'));

          image.src = command.url;
          document.body.append(image);
        });
        result = { ok: true };
      } else {
        const response = await fetch(command.url, { cache: 'no-store' });
        const text = await response.text();
        result = {
          ok: true,
          status: response.status,
          text,
          edited: response.headers.get('x-intercepted')
        };
      }
    } catch {
      result = { ok: false };
    }
    void report({ kind: 'result', nonce: command.nonce, ...result });
  }
  await new Promise((resolve) => setTimeout(resolve, 20));
}
