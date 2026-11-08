/** Main-only asynchronous OS source metadata. Source strings alone never grant renderer media access. */
export default function createDesktopCapture(
  native,
  isReady,
  isExited,
  error,
  unsupported,
  createEmpty
) {
  const pending = new Map();
  const limit = 8;
  const deadline = 30000;
  const byteLength = globalThis.Buffer.byteLength.bind(globalThis.Buffer);
  let next = 0;
  let stopped = false;

  function active() {
    if (stopped || isExited()) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Desktop capture application has exited');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'desktopCapturer requires app readiness');
    }
  }

  function options(value) {
    if (!value || typeof value !== 'object') {
      throw new Error('Invalid options');
    }
    const types = value.types;
    if (!Array.isArray(types)) {
      throw new Error('Invalid options');
    }
    const length = types.length;
    if (length > 64) {
      throw error('ERR_OUT_OF_RANGE', 'Too many desktop capture types');
    }
    const selected = new Set();
    for (let index = 0; index < length; ++index) {
      const type = types[index];
      if (type !== 'screen' && type !== 'window') {
        throw new Error('Invalid options');
      }
      selected.add(type);
    }
    const size = value.thumbnailSize;
    let width = 150;
    let height = 150;
    if (size !== undefined) {
      if (!size || typeof size !== 'object') {
        throw new Error('Invalid options');
      }
      width = size.width;
      height = size.height;
      if (
        !Number.isInteger(width) ||
        !Number.isInteger(height) ||
        width < 0 ||
        height < 0 ||
        width > 16384 ||
        height > 16384
      ) {
        throw new Error('Invalid options');
      }
    }
    if (width !== 0 && height !== 0) {
      unsupported('desktopCapturer thumbnails');
    }
    const icons = value.fetchWindowIcons;
    if (icons !== undefined && typeof icons !== 'boolean') {
      throw new Error('Invalid options');
    }
    if (icons) {
      unsupported('desktopCapturer window icons');
    }
    for (const key of Object.keys(value)) {
      if (!['types', 'thumbnailSize', 'fetchWindowIcons'].includes(key)) {
        unsupported(`desktopCapturer option ${key}`);
      }
    }
    return selected;
  }

  function cancel(id) {
    try {
      native.desktopCaptureCancel(id);
    } catch {
      // Shutdown must still settle all local promises if native teardown began.
    }
  }

  async function getSources(value) {
    const types = options(value);
    // Option getters can synchronously exit the app. Recheck before admission.
    active();
    if (pending.size >= limit) {
      throw error('ERR_FIREFLY_CAPTURE_LIMIT', 'Too many pending desktop capture requests');
    }
    if (next >= 2147483647) {
      throw error('ERR_FIREFLY_CAPTURE_LIMIT', 'Desktop capture request IDs exhausted');
    }
    const id = ++next;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (!pending.delete(id)) {
          return;
        }
        cancel(id);
        reject(error('ERR_FIREFLY_CAPTURE_TIMEOUT', 'OS source enumeration timed out'));
      }, deadline);
      pending.set(id, { resolve, reject, timer, types });
      try {
        native.desktopCaptureStart(id);
      } catch (failure) {
        pending.delete(id);
        clearTimeout(timer);
        reject(failure);
      }
    });
  }

  function snapshot(records, types) {
    if (!Array.isArray(records) || records.length > 1024) {
      throw error('ERR_FIREFLY_CAPTURE_DATA', 'Invalid native desktop source list');
    }
    const sources = [];
    const seen = new Set();
    let budget = 0;
    for (const record of records) {
      const id = record?.id;
      const name = record?.name;
      const display = record?.display_id;
      if (
        typeof id !== 'string' ||
        !/^(screen|window):[1-9][0-9]{0,19}:0$/.test(id) ||
        typeof name !== 'string' ||
        name.includes('\0') ||
        byteLength(name, 'utf8') > 4096 ||
        typeof display !== 'string' ||
        !/^(?:[1-9][0-9]{0,19})?$/.test(display) ||
        seen.has(id)
      ) {
        throw error('ERR_FIREFLY_CAPTURE_DATA', 'Invalid native desktop source');
      }
      const kind = id.split(':')[0];
      if (
        (kind === 'screen' &&
          (display === '' || display !== id.split(':')[1] || !id.endsWith(':0'))) ||
        (kind === 'window' && display !== '')
      ) {
        throw error('ERR_FIREFLY_CAPTURE_DATA', 'Native source/display identity mismatch');
      }
      seen.add(id);
      budget += byteLength(name, 'utf8') + id.length + display.length;
      if (budget > 1048576) {
        throw error('ERR_FIREFLY_CAPTURE_DATA', 'Native desktop metadata exceeds its budget');
      }
      if (types.has(kind)) {
        sources.push({ id, name, display_id: display, thumbnail: createEmpty(), appIcon: null });
      }
    }
    return sources;
  }

  function dispatch(value) {
    const request = pending.get(value?.id);
    if (!request) {
      return;
    }
    pending.delete(value.id);
    clearTimeout(request.timer);
    try {
      active();
      if (value.code) {
        throw error(value.code, value.message || 'Native source enumeration failed');
      }
      const sources = snapshot(value.sources, request.types);
      active();
      request.resolve(sources);
    } catch (failure) {
      request.reject(failure);
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const requests = [...pending];
    pending.clear();
    for (const [id, request] of requests) {
      clearTimeout(request.timer);
      cancel(id);
      request.reject(
        error('ERR_FIREFLY_APP_EXITED', 'Application exited during source enumeration')
      );
    }
  }

  return { desktopCapturer: { getSources }, dispatch, stop };
}
