import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'device-access',
  requires: [
    'session.defaultSession.setDevicePermissionHandler',
    'session.defaultSession.setPermissionCheckHandler',
    'dialog.showMessageBox'
  ],
  earlyRequires: [],
  smoke: false,
  setup: async ({ api, window, directory, smoke, pageURL }) => {
    const session = window.webContents.session;
    session.setPermissionCheckHandler(
      (contents, permission, origin) =>
        contents === window.webContents &&
        permission === 'hid' &&
        origin === new URL(pageURL).origin
    );
    session.setDevicePermissionHandler(
      (details) => details.deviceType === 'hid' && details.origin === new URL(pageURL).origin
    );
    session.on('select-hid-device', (event, details, callback) => {
      event.preventDefault();
      const devices = details.deviceList;
      api.dialog
        .showMessageBox(window, {
          message: 'Choose a HID device',
          buttons: ['Cancel', ...devices.map((device) => device.productName || device.deviceId)],
          cancelId: 0,
          defaultId: 0
        })
        .then(
          ({ response }) =>
            callback(
              response > 0 && response <= devices.length ? devices[response - 1].deviceId : ''
            ),
          () => callback('')
        );
    });
  }
});
