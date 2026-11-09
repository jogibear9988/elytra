/** Each document authors its own reports/navigation; no borrowed child realm bridge calls. */
const params = new URL(location.href).searchParams;
const tag = params.get('tag') ?? 'top';
const revision = params.get('revision') ?? '1';

const report = (value) =>
  fetch('/report', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ tag, revision, url: location.href, ...value })
  });

let navigated = false;
let hashed = false;
let invoked = false;
try {
  await report({ kind: 'ready' });
  if (tag === 'top') {
    for (const [name, origin] of [
      ['same', location.origin],
      ['cross', params.get('cross')]
    ]) {
      const frame = document.createElement('iframe');
      frame.name = name;
      frame.src = origin + '/page.html?tag=' + name;
      document.body.append(frame);
    }
  }
  while (true) {
    const command = await (await fetch('/commands')).json();
    if (command.navigate && tag === 'same' && revision === '1' && !navigated) {
      navigated = true;
      location.href = location.origin + '/page.html?tag=same&revision=2';
      break;
    }
    if (command.hash && tag === 'top' && !hashed) {
      hashed = true;
      location.hash = 'retained';
      await report({ kind: 'hash' });
    }
    if (command.ipc && tag === 'top' && !invoked) {
      invoked = true;
      await frameFixture.identify();
      await report({ kind: 'ipc' });
    }
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
} catch (failure) {
  await report({ kind: 'error', error: String(failure) });
}
