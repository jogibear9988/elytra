/** Main-process display snapshots and Electron-style geometry/event policy. */
export default function createScreen(native, EventEmitter, isReady, error, unsupported) {
  const parse = JSON.parse;

  const clone = (value) => JSON.parse(JSON.stringify(value));

  const eventNames = ['display-added', 'display-removed', 'display-metrics-changed'];
  const screen = new EventEmitter();
  let baseline = null;

  function snapshot() {
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'screen requires app.whenReady()');
    }
    const value = parse(native.screenSnapshot());
    if (!baseline) {
      baseline = value;
    }
    return value;
  }

  function primary(value) {
    const display = value.displays.find((display) => display.id === value.primaryId);
    if (!display) {
      throw error('ERR_FIREFLY_DISPLAY_UNAVAILABLE', 'No primary display is available');
    }
    return display;
  }

  function integer(value, name) {
    if (
      typeof value !== 'number' ||
      !Number.isInteger(value) ||
      value < -2147483648 ||
      value > 2147483647
    ) {
      throw error('ERR_INVALID_ARG_TYPE', `${name} must be a signed 32-bit integer`);
    }
    return value;
  }

  function point(value) {
    if (!value || typeof value !== 'object') {
      throw error('ERR_INVALID_ARG_TYPE', 'Expected a Point');
    }
    return { x: integer(value.x, 'x'), y: integer(value.y, 'y') };
  }

  function rectangle(value) {
    const result = point(value);
    result.width = integer(value.width, 'width');
    result.height = integer(value.height, 'height');
    if (result.width < 0 || result.height < 0) {
      throw error('ERR_OUT_OF_RANGE', 'Rectangle dimensions must be nonnegative');
    }
    return result;
  }

  function distance(point, rect) {
    const dx = Math.max(rect.x - point.x, 0, point.x - rect.x - rect.width);
    const dy = Math.max(rect.y - point.y, 0, point.y - rect.y - rect.height);
    return dx * dx + dy * dy;
  }

  function nearest(value, position) {
    let best = primary(value);
    let minimum = distance(position, best.bounds);
    for (const display of value.displays) {
      const current = distance(position, display.bounds);
      if (current < minimum) {
        minimum = current;
        best = display;
      }
    }
    return best;
  }

  screen.getAllDisplays = () => clone(snapshot().displays);

  screen.getPrimaryDisplay = () => clone(primary(snapshot()));

  screen.getCursorScreenPoint = () => {
    primary(snapshot());
    return parse(native.cursorScreenPoint());
  };

  screen.getDisplayNearestPoint = (value) => {
    const current = snapshot();
    return clone(nearest(current, point(value)));
  };

  screen.getDisplayMatching = (value) => {
    const current = snapshot();
    const rect = rectangle(value);
    let best = primary(current);
    let maximum = 0;
    for (const display of [best, ...current.displays.filter((display) => display.id !== best.id)]) {
      const b = display.bounds;
      const area =
        Math.max(0, Math.min(rect.x + rect.width, b.x + b.width) - Math.max(rect.x, b.x)) *
        Math.max(0, Math.min(rect.y + rect.height, b.y + b.height) - Math.max(rect.y, b.y));
      if (area > maximum) {
        maximum = area;
        best = display;
      }
    }
    if (!maximum) {
      best = nearest(current, { x: rect.x + rect.width / 2, y: rect.y + rect.height / 2 });
    }
    return clone(best);
  };

  for (const name of [
    'screenToDipPoint',
    'dipToScreenPoint',
    'screenToDipRect',
    'dipToScreenRect'
  ]) {
    screen[name] = () => unsupported(`screen.${name}`);
  }

  screen.on('newListener', (name) => {
    if (isReady() && eventNames.includes(name)) {
      snapshot();
    }
  });

  function ready() {
    if (eventNames.some((name) => screen.listenerCount(name))) {
      snapshot();
    }
  }

  function changed() {
    if (!isReady() || !baseline) {
      return;
    }
    const next = snapshot();
    const previous = baseline;
    baseline = next; // Commit before callbacks so reentrant reads cannot duplicate events.
    const old = new Map(previous.displays.map((display) => [display.id, display]));
    const fresh = new Map(next.displays.map((display) => [display.id, display]));
    for (const display of previous.displays) {
      if (!fresh.has(display.id)) {
        screen.emit('display-removed', {}, clone(display));
      }
    }
    for (const display of next.displays) {
      const prior = old.get(display.id);
      if (!prior) {
        screen.emit('display-added', {}, clone(display));
      } else {
        const metrics = ['bounds', 'workArea', 'scaleFactor', 'rotation'].filter(
          (key) => JSON.stringify(display[key]) !== JSON.stringify(prior[key])
        );
        if (metrics.length) {
          screen.emit('display-metrics-changed', {}, clone(display), metrics);
        }
      }
    }
  }

  return { screen, ready, changed };
}
