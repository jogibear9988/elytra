// Enabled only by the sample's explicit --smoke mode. This is an app-local test
// report, not a privileged IPC API; production mode has no report endpoint.
(async () => {
  const token = new URL(import.meta.url).searchParams.get('token');

  function stage(name) {
    fetch('/__firefly_smoke_stage?token=' + encodeURIComponent(token) + '&stage=' + name).catch(
      () => {}
    );
  }

  async function report(result) {
    await fetch('/__firefly_smoke_result?token=' + encodeURIComponent(token), {
      method: 'POST',
      body: JSON.stringify(result)
    });
  }

  try {
    await customElements.whenDefined('node-projects-app-shell');
    stage('defined');

    function find(root, selector) {
      const match = root.querySelector(selector);
      if (match) {
        return match;
      }
      for (const element of root.querySelectorAll('*')) {
        if (element.shadowRoot) {
          const result = find(element.shadowRoot, selector);
          if (result) {
            return result;
          }
        }
      }
      return null;
    }

    let container;
    for (let i = 0; i < 80 && !container; i++) {
      container = find(document, 'node-projects-document-container');
      if (!container) {
        await new Promise((resolve) => setTimeout(resolve, 250));
      }
    }
    if (!container) {
      throw Error('Designer document container did not appear');
    }
    stage('container');
    await container.whenReady();
    stage('ready');
    await container.setContentAsync('<button id="firefly-smoke-button">Firefly smoke</button>');
    await container.commitPendingChanges();
    if (
      !container.content.includes('firefly-smoke-button') ||
      !container.content.includes('Firefly smoke')
    ) {
      throw Error('Document editing did not round-trip');
    }
    stage('edited');
    container.currentView = 'code';
    await new Promise((resolve) => setTimeout(resolve, 1000));
    if (!find(document, '.monaco-editor')) {
      throw Error('Monaco editor is missing');
    }
    stage('monaco');
    if (typeof process !== 'undefined' || typeof require !== 'undefined') {
      throw Error('Unexpected renderer privileges');
    }
    if (!window.designerWindow) {
      throw Error('Window controls missing');
    }
    let geometry = false;
    for (let attempt = 0; attempt < 30 && !geometry; attempt++) {
      if (typeof __fireflyRegionSnapshot === 'function') {
        const regions = JSON.parse(__fireflyRegionSnapshot());
        const button = document.getElementById('window-maximize').getBoundingClientRect();
        const x = button.left + button.width / 2;
        const y = button.top + button.height / 2;
        geometry =
          regions.some((r) => r[4]) &&
          regions.some(
            (r) => !r[4] && x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]
          );
      }
      if (!geometry) {
        await new Promise((resolve) => setTimeout(resolve, 100));
      }
    }
    if (!geometry) {
      throw Error('External toolbar drag/no-drag geometry is missing');
    }
    stage('geometry');

    const waitFor = async (predicate) => {
      for (let i = 0; i < 50; i++) {
        if (predicate(await designerWindow.state())) {
          return;
        }
        await new Promise((r) => setTimeout(r, 100));
      }
      throw Error('Window state timeout');
    };

    document.getElementById('window-maximize').click();
    await waitFor((s) => s.maximized);
    await new Promise((r) => setTimeout(r, 100));
    document.getElementById('window-maximize').click();
    await waitFor((s) => !s.maximized);
    document.getElementById('window-minimize').click();
    // A minimized renderer may suspend fetch/timer completion. The test owner
    // observes the real minimize event and restores natively; production does not.
    await new Promise((resolve) => setTimeout(resolve, 750));
    await waitFor((s) => !s.minimized);
    const bad = await fetch('/__firefly_window/maximize', {
      method: 'POST',
      headers: { 'X-Firefly-Window-Token': 'wrong' }
    });
    if (bad.status !== 403) {
      throw Error('Window capability was not checked');
    }
    stage('controls');
    await report({
      windowControls: true,
      ready: true,
      edited: true,
      monaco: true,
      privilegesAbsent: true
    });
    stage('close');
    document.getElementById('window-close').click();
  } catch (error) {
    await report({ ready: false, error: String(error) });
  }
})();
