const { app, BrowserWindow } = require(process.env.FIREFLY_API_MODULE || 'firefly');
const trace = [];

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

let beforeCount = 0;
let willCount = 0;

app.on('window-all-closed', () => trace.push('window-all-closed'));

app.on('before-quit', (event) => {
  trace.push(`before:${++beforeCount}`);
  if (beforeCount === 1) {
    event.preventDefault();
  }
});

app.on('will-quit', (event) => {
  trace.push(`will:${++willCount}`);
  if (willCount === 1) {
    event.preventDefault();

    setTimeout(() => {
      check(app.isReady(), 'cancelled quit must preserve app readiness');
      check(BrowserWindow.getAllWindows().length === 0, 'closed windows must leave registry');
      const replacement = new BrowserWindow({ show: false });

      replacement.on('closed', () => trace.push('replacement-closed'));

      trace.push('replacement-created');
      app.quit();
    }, 5);
  }
});

app.on('quit', (_event, code) => {
  trace.push(`quit:${code}`);
  console.log(JSON.stringify({ trace }));
});

app.whenReady().then(() => {
  const win = new BrowserWindow({ show: false });
  let attempts = 0;

  win.on('close', (event) => {
    trace.push(`close:${++attempts}`);
    if (attempts === 1) {
      event.preventDefault();
    }
  });

  win.on('closed', () => trace.push('closed'));

  app.quit();
  check(!win.isDestroyed(), 'cancelled before-quit must keep window');
  check(attempts === 0, 'before-quit must precede close');
  app.quit();
  check(!win.isDestroyed(), 'cancelled close must keep window');
  check(willCount === 0, 'cancelled close must not enter will-quit');

  setTimeout(() => app.quit(), 5);
});
