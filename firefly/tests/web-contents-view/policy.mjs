/** Input/ownership policy only: the projection below is not a native renderer/widget test. */
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import createViews from '../../core/views/index.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

function sameChildren(actual, expected) {
  assert.equal(actual.length, expected.length);
  for (let index = 0; index < actual.length; index++) {
    assert.equal(actual[index], expected[index]);
  }
}

function setup(hooks = {}) {
  const calls = [];
  const values = new Map();
  let id = 100;
  let api;
  class Emitter extends EventEmitter {
    constructor() {
      super();
      hooks.emitter?.(api);
    }
  }
  const native = {
    createView(preload, args) {
      calls.push(['create', preload, args]);
      const next = ++id;
      values.set(next, { bounds: [0, 0, 0, 0], visible: true });
      return next;
    },

    attachView(...args) {
      calls.push(['attach', ...args]);
      if (hooks.failAttach) {
        throw error('TEST_ATTACH', 'Native projection refused attach');
      }
    },

    detachView(...args) {
      calls.push(['detach', ...args]);
      if (hooks.failDetach) {
        throw error('TEST_DETACH', 'Native projection refused detach');
      }
    },

    setViewBounds(key, ...bounds) {
      values.get(key).bounds = bounds;
      calls.push(['bounds', key]);
    },

    viewBounds(key) {
      return JSON.stringify(values.get(key).bounds);
    },

    setViewVisible(key, visible) {
      values.get(key).visible = visible;
      calls.push(['visible', key]);
    },

    viewVisible(key) {
      return values.get(key).visible;
    },

    closeContents(key) {
      calls.push(['close', key]);
    }
  };
  api = createViews(
    native,
    Emitter,
    (host) => {
      const contents = new EventEmitter();
      contents.host = host;
      return contents;
    },
    (prefs) => {
      hooks.preferences?.(api);
      return { preload: prefs.preload ?? '', additionalArguments: prefs.additionalArguments ?? [] };
    },
    () => {
      if (hooks.notReady) {
        throw error('ERR_FIREFLY_NOT_READY', 'Not ready');
      }
    },
    error,
    unsupported
  );
  return { api, calls, hooks };
}

{
  const { api, hooks, calls } = setup();
  const first = { id: 1 };
  const second = { id: 2 };
  const root = api.contentView(first);
  const other = api.contentView(second);
  assert.equal(api.contentView(first), root);
  assert.deepEqual(root.children, []);
  const a = new api.WebContentsView();
  const b = new api.WebContentsView();
  root.addChildView(a);
  root.addChildView(b);
  const snapshot = root.children;
  root.addChildView(a);
  sameChildren(root.children, [b, a]);
  sameChildren(snapshot, [a, b]);
  snapshot.length = 0;
  sameChildren(root.children, [b, a]);
  hooks.failAttach = true;

  assert.throws(() => other.addChildView(a), { code: 'TEST_ATTACH' });

  assert.equal(api.ownerOf(a.webContents), first);
  sameChildren(root.children, [b, a]);
  assert.deepEqual(other.children, []);
  hooks.failAttach = false;
  other.addChildView(a, 100);
  assert.equal(api.ownerOf(a.webContents), second);
  sameChildren(root.children, [b]);
  sameChildren(other.children, [a]);
  api.detachWindow(first);
  assert.equal(api.ownerOf(a.webContents), second);
  const before = calls.length;
  root.removeChildView(a);
  assert.equal(calls.length, before);
  hooks.failDetach = true;

  assert.throws(() => other.removeChildView(a), { code: 'TEST_DETACH' });

  assert.equal(api.ownerOf(a.webContents), second);
  hooks.failDetach = false;
  other.removeChildView(a);
  assert.equal(api.ownerOf(a.webContents), null);
  console.log(
    'PASS view policy native-refusal metadata atomicity, reparent/reorder and independent snapshots'
  );
}

{
  const { api } = setup();
  const view = new api.WebContentsView();
  assert.deepEqual(view.getBounds(), { x: 0, y: 0, width: 0, height: 0 });
  view.setBounds({ x: 1.6, y: -1.6, width: -3, height: 9.7 });
  assert.deepEqual(view.getBounds(), { x: 2, y: -2, width: 0, height: 10 });
  view.setBounds({ x: -2147483648, y: 2147483647, width: 0, height: 0 });
  assert.equal(view.getBounds().x, -2147483648);
  for (const bounds of [
    { x: 2147483647, y: 0, width: 1, height: 0 },
    { x: 0, y: 0, width: 16385, height: 0 }
  ]) {
    assert.throws(() => view.setBounds(bounds), { code: 'ERR_INVALID_ARG_VALUE' });
  }
  for (const bounds of [
    { x: NaN, y: 0, width: 0, height: 0 },
    { x: Infinity, y: 0, width: 0, height: 0 },
    { y: 0, width: 0, height: 0 },
    Object.assign(Object.create({ x: 0 }), { y: 0, width: 0, height: 0 })
  ]) {
    assert.throws(() => view.setBounds(bounds), { code: 'ERR_INVALID_ARG_TYPE' });
  }
  let read = false;
  const accessor = {
    get x() {
      read = true;
      return 0;
    },

    y: 0,
    width: 0,
    height: 0
  };

  assert.throws(() => view.setBounds(accessor), { code: 'ERR_INVALID_ARG_TYPE' });

  assert.equal(read, false);

  assert.throws(() => view.setBounds({ x: 0, y: 0, width: 0, height: 0, animate: true }), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });

  assert.throws(() => view.setBounds({ x: 0, y: 0, width: 0, height: 0 }, {}), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });

  assert.equal(view.getVisible(), true);
  view.setVisible(false);
  assert.equal(view.getVisible(), false);

  assert.throws(() => view.setVisible(0), { code: 'ERR_INVALID_ARG_TYPE' });

  console.log(
    'PASS view policy finite own bounds, rounding/clamping, native edge limits and visibility'
  );
}

{
  const { api, hooks, calls } = setup();
  const view = new api.WebContentsView();
  const root = api.contentView({ id: 1 });

  assert.throws(() => new api.WebContentsView({ webContents: view.webContents }), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });

  assert.throws(() => new api.WebContentsView({ other: true }), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });

  assert.throws(
    () =>
      new api.WebContentsView({
        get webPreferences() {
          throw Error('Getter executed');
        }
      }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );

  assert.throws(() => new (class extends api.WebContentsView {})(), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });

  assert.throws(() => view.getBounds.call({}), { code: 'ERR_INVALID_THIS' });

  assert.throws(() => root.addChildView({}), { code: 'ERR_INVALID_THIS' });

  for (const index of [-1, 0.5, NaN]) {
    assert.throws(() => root.addChildView(view, index), { code: 'ERR_INVALID_ARG_VALUE' });
  }
  for (const method of [
    'setBackgroundColor',
    'setBorderRadius',
    'addChildView',
    'removeChildView'
  ]) {
    assert.throws(() => view[method](), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });
  }

  assert.throws(() => root.getBounds(), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

  hooks.notReady = true;
  const before = calls.length;

  assert.throws(() => new api.WebContentsView(), { code: 'ERR_FIREFLY_NOT_READY' });

  assert.equal(calls.length, before);
  console.log(
    'PASS view policy forged receivers, unsupported adoption/features, readiness and invalid child indices'
  );
}

{
  const { api, calls } = setup();
  const win = { id: 1 };
  const root = api.contentView(win);
  const view = new api.WebContentsView();
  root.addChildView(view);
  const contents = view.webContents;
  const host = contents.host;
  let contentsClosed = 0;
  let hostClosed = 0;

  contents.on('destroyed', () => {
    contentsClosed++;
    assert.equal(api.records.has(host.id), false);
    assert.equal(api.ownerOf(contents), null);
    assert.deepEqual(root.children, []);
    assert.equal(view.webContents, undefined);
  });

  host.on('closed', () => hostClosed++);

  host.destroy();
  assert.equal(api.ownerOf(contents), null);

  assert.throws(() => root.addChildView(view), { code: 'ERR_FIREFLY_OBJECT_DESTROYED' });

  assert.equal(calls.filter((call) => call[0] === 'close').length, 1);
  host.destroy();
  api.destroyed(host.id);
  api.destroyed(host.id);
  assert.equal(contentsClosed, 1);
  assert.equal(hostClosed, 1);

  assert.throws(() => view.getBounds(), { code: 'ERR_FIREFLY_OBJECT_DESTROYED' });

  console.log(
    'PASS view policy closing/stale endpoints and exactly-once destruction after metadata cleanup'
  );
}

{
  const { api, calls } = setup();
  const win = { id: 1 };
  const root = api.contentView(win);
  const view = new api.WebContentsView();
  root.addChildView(view);
  const contents = view.webContents;
  const host = contents.host;
  const before = calls.length;
  api.shutdown();
  api.shutdown();
  host.destroy();
  assert.equal(calls.length, before);
  assert.equal(api.records.size, 0);
  assert.equal(api.ownerOf(contents), null);
  assert.equal(view.webContents, undefined);

  assert.throws(() => root.children, { code: 'ERR_FIREFLY_OBJECT_DESTROYED' });

  assert.throws(() => new api.WebContentsView(), { code: 'ERR_FIREFLY_APP_EXITED' });

  console.log('PASS view policy shutdown retires ownership without native calls');
}

for (const source of ['preferences', 'emitter']) {
  let count = 0;
  const hooks = {};
  const context = setup(hooks);

  hooks[source] = (api) => {
    count++;
    if (source === 'preferences' || count === 2) {
      api.shutdown();
    }
  };

  assert.throws(() => new context.api.WebContentsView(), { code: 'ERR_FIREFLY_APP_EXITED' });

  assert.equal(context.calls.length, 0);
  assert.equal(context.api.records.size, 0);
  console.log(`PASS view policy ${source} reentry cannot allocate after shutdown`);
}
console.log('DONE view policy');
