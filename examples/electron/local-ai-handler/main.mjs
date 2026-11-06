import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'local-ai-handler',
  requires: ['utilityProcess.fork'],
  earlyRequires: [],
  smoke: false,
  options: { webPreferences: { enableBlinkFeatures: 'AIPromptAPI' } },
  setup: async ({ api, window, directory, smoke, pageURL }) => {
    if (!process.env.ELYTRA_LOCAL_AI_MODULE)
      throw Object.assign(new Error('Configure ELYTRA_LOCAL_AI_MODULE'), {
        code: 'ERR_EXAMPLE_CONFIGURATION'
      });
    const { join } = await import('node:path');
    const worker = api.utilityProcess.fork(join(directory, 'utility.mjs'));
    window.webContents.session.registerLocalAIHandler(worker);
    api.app.on('will-quit', () => worker.kill());
  }
});
