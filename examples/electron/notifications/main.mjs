/**
 * Manual notification request example. Firefly's API is partial; actual OS permission,
 * visible delivery and interaction remain unverified. Automated checks never post alerts.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'notifications',
  requires: ['Notification.isSupported'],
  earlyRequires: [],
  smoke: false,

  setup({ window, state }) {
    window.on('closed', () => {
      state.cancelNotification?.();
      state.notification?.close();
    });
  },

  actions: {
    notify: ({ api, state }) => {
      if (!api.Notification.isSupported()) {
        throw Error(
          'Notifications unavailable on this host or application bundle; see docs/examples/notifications.md'
        );
      }
      if (state.cancelNotification) {
        throw Error('A notification request is already waiting for native acknowledgement');
      }
      state.notification?.close();
      const notification = new api.Notification({
        title: 'Elytra example',
        body: 'A native notification requested by the local app.'
      });
      state.notification = notification;
      return new Promise((resolve, reject) => {
        let settled = false;

        const cleanup = () => {
          clearTimeout(timer);
          notification.removeListener('show', accepted);
          notification.removeListener('failed', failed);
          notification.removeListener('close', closed);
          state.cancelNotification = null;
        };

        const finish = (error) => {
          if (settled) {
            return;
          }
          settled = true;
          cleanup();
          if (error) {
            try {
              notification.close();
            } catch (closeError) {
              error = Error(error.message + '; native cleanup failed: ' + closeError.message);
            }
            reject(error);
          } else {
            resolve({ nativeRequestAccepted: true, osVisibility: 'unverified' });
          }
        };

        const accepted = () => finish();

        const failed = (_event, message) =>
          finish(Error(String(message || 'Native notification request failed')));

        const closed = () => finish(Error('Notification closed before native acknowledgement'));

        const timer = setTimeout(
          () => finish(Error('Native notification acknowledgement timed out after 10 seconds')),
          10000
        );

        state.cancelNotification = () =>
          finish(Error('Notification window closed before acknowledgement'));
        // Install both outcome listeners before show() can submit or synchronously fail.
        notification.once('show', accepted);
        notification.once('failed', failed);
        notification.once('close', closed);
        try {
          notification.show();
        } catch (error) {
          finish(error);
        }
      });
    }
  }
});
