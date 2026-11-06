/**
 * Run one explicitly selected desktop runtime with capability checks and bounded smoke
 * observations.
 */
import process from 'node:process';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { serveAssets } from './assets.mjs';

export async function runExample(source, config) {
  let api;

  const fail = (error) => {
    console.error(`${error.code || 'ERR_EXAMPLE'}: ${config.id}: ${error.message}`);
    if (api?.app) {
      api.app.exit(1);
    } else {
      process.exit(1);
    }
  };

  const requireAPI = (names) => {
    for (const name of names) {
      const value = name.split('.').reduce((owner, key) => owner?.[key], api);
      if (value === undefined) {
        throw Object.assign(new Error(`Missing API ${name}; see docs/examples/${config.id}.md`), {
          code: 'ERR_FIREFLY_UNSUPPORTED_API'
        });
      }
    }
  };

  try {
    const { runtime } = JSON.parse(
      await readFile(new URL('../runtime.json', import.meta.url), 'utf8')
    );
    if (!['firefly', 'electron'].includes(runtime)) {
      throw Error('Unknown example runtime');
    }
    api = await import(runtime);
    process.env.ELYTRA_EXAMPLE_RUNTIME = runtime;
    if (config.platforms && !config.platforms.includes(process.platform)) {
      throw Object.assign(new Error(`Requires ${config.platforms.join('/')}`), {
        code: 'ERR_EXAMPLE_PLATFORM'
      });
    }
    requireAPI(config.earlyRequires || []);
    if (config.beforeReady && (await config.beforeReady(api)) === false) {
      return;
    }
    const smoke = process.argv.includes('--smoke');

    api.app.on('window-all-closed', () => api.app.quit());
    // Resolve this module before readiness: awaiting app.whenReady at top level
    // can prevent Electron's ESM startup from ever reaching the ready event.
    api.app
      .whenReady()
      .then(async () => {
        requireAPI(config.requires || []);
        const actions = config.actions || {};
        if (Object.keys(actions).length) {
          requireAPI(['ipcMain.handle']);
        }
        const options = { width: 900, height: 650, title: config.id, ...config.options };
        if (config.preload || Object.keys(actions).length) {
          options.webPreferences = {
            contextIsolation: true,
            nodeIntegration: false,
            sandbox: true,
            ...options.webPreferences,
            preload: fileURLToPath(new URL('./preload.cjs', source))
          };
        }
        const window = new api.BrowserWindow(options);
        for (const name of config.instanceRequires || []) {
          if (name.split('.').reduce((owner, key) => owner?.[key], window) === undefined) {
            throw Object.assign(new Error(`Missing API BrowserWindow.${name}`), {
              code: 'ERR_FIREFLY_UNSUPPORTED_API'
            });
          }
        }
        const page = fileURLToPath(new URL('./index.html', source));
        const assets = await serveAssets(source, config.id);
        const pageURL = assets.url;
        process.env.ELYTRA_EXAMPLE_ORIGIN = new URL(pageURL).origin;

        api.app.on('quit', () => assets.server.close());

        const allowedPages = new Set([pageURL, new URL('./second.html', pageURL).href]);
        const context = {
          api,
          window,
          page,
          pageURL,
          directory: fileURLToPath(new URL('./', source)),
          smoke,
          state: Object.create(null)
        };

        const authorized = (event) => {
          if (
            event.sender !== window.webContents ||
            event.senderFrame !== window.webContents.mainFrame ||
            !allowedPages.has(event.senderFrame.url.split('#')[0])
          ) {
            throw Error('Unexpected IPC sender');
          }
        };

        if (Object.keys(actions).length) {
          api.ipcMain.handle(`elytra-example:${config.id}`, (event, action, value) => {
            authorized(event);
            if (!Object.hasOwn(actions, action)) {
              throw Error('Unknown example action');
            }
            return actions[action](context, value);
          });

          window.on('closed', () => api.ipcMain.removeHandler(`elytra-example:${config.id}`));
        }
        if (config.setup) {
          await config.setup(context);
        }
        await window.loadURL(pageURL);
        if (smoke) {
          if (!config.smoke) {
            throw Object.assign(
              new Error(
                'Requires the documented manual validation; startup is not feature acceptance'
              ),
              { code: 'ERR_EXAMPLE_MANUAL_VALIDATION' }
            );
          }
          for (let i = 0; i < 500; ++i) {
            const title = window.webContents.getTitle();
            if (title === 'EXAMPLE_READY') {
              break;
            }
            if (title.startsWith('EXAMPLE_ERROR')) {
              throw Error(title);
            }
            await new Promise((resolve) => setTimeout(resolve, 10));
          }
          if (window.webContents.getTitle() !== 'EXAMPLE_READY') {
            throw Error('Page did not complete its assertions');
          }
          if (config.verify) {
            await config.verify(context);
          }
          console.log(JSON.stringify({ example: config.id, ready: true }));
          window.destroy();
          api.app.quit();
        }
      })
      .catch(fail);
  } catch (error) {
    fail(error);
  }
}
