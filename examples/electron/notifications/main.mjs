import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'notifications',
  requires: ['Notification.isSupported'],
  earlyRequires: [],
  smoke: false,
  actions: {
    notify: ({ api }) => {
      if (!api.Notification.isSupported()) throw Error('Notifications unavailable on this host');
      new api.Notification({
        title: 'Elytra example',
        body: 'A native notification requested by the local app.'
      }).show();
    }
  }
});
