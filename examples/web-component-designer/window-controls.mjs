// Sample-local capability for this one window, not a general Firefly IPC API.
(() => {
  const controls = window.designerWindow;
  if (!controls) {
    return;
  }

  async function command(action) {
    const state = await controls[action]();
    const maximize = document.getElementById('window-maximize');
    maximize.setAttribute('aria-label', state.maximized ? 'Restore window' : 'Maximize window');
    maximize.title = state.maximized ? 'Restore window' : 'Maximize window';
    maximize.textContent = state.maximized ? '❐' : '□';
    return state;
  }

  document
    .getElementById('window-minimize')
    .addEventListener('click', () => command('minimize').catch(console.error));

  document.getElementById('window-maximize').addEventListener('click', async () => {
    try {
      const state = await controls.state();
      await command(state.maximized ? 'unmaximize' : 'maximize');
    } catch (error) {
      console.error(error);
    }
  });

  document
    .getElementById('window-close')
    .addEventListener('click', () => command('close').catch(console.error));

  command('state').catch(console.error);
})();
