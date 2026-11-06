const { app, BrowserWindow } = require(process.env.FIREFLY_API_MODULE || 'firefly');

const check = (ok, message) => {
  if (!ok) {
    throw Error(message);
  }
};

const wait = () => new Promise((resolve) => setTimeout(resolve, 200));

app
  .whenReady()
  .then(async () => {
    const win = new BrowserWindow({ frame: false, width: 640, height: 400 });
    const events = [];
    for (const name of ['maximize', 'unmaximize', 'minimize', 'restore']) {
      win.on(name, () => events.push(name));
    }
    check(!win.isMinimized() && !win.isMaximized(), 'initial state');
    win.maximize();
    await wait();
    check(win.isMaximized(), 'maximize');
    win.unmaximize();
    await wait();
    check(!win.isMaximized(), 'unmaximize');
    win.minimize();
    await wait();
    check(win.isMinimized(), 'minimize');
    win.restore();
    await wait();
    check(!win.isMinimized(), 'restore');
    if (process.env.FIREFLY_API_MODULE !== 'electron') {
      for (const name of ['maximize', 'unmaximize', 'minimize', 'restore']) {
        check(events.includes(name), 'event ' + name);
      }
    }
    console.log(JSON.stringify({ frameless: true, minimize: true, maximize: true, restore: true }));

    win.on('closed', () => {
      let denied = false;
      try {
        win.minimize();
      } catch {
        denied = true;
      }
      check(denied, 'destroyed controls reject');
      app.quit();
    });

    win.destroy();
  })
  .catch((error) => {
    console.error(error.stack);
    app.exit(1);
  });
