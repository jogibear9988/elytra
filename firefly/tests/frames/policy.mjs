/** Actual frame facade with controlled canonical native documents, never fabricated public IPC frames. */
import create from '../../core/frames/index.mjs';

const error = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
}

const row = (token, document, url, parent = null) => ({
  token,
  document,
  frameTreeNodeId: token + 100,
  processId: 24,
  osProcessId: 4242,
  routingId: token + 200,
  url,
  origin: new URL(url).origin,
  parent,
  top: 1
});

function fixture() {
  const state = {
    alive: true,
    stopped: false,
    snapshot: {
      contentsId: 7,
      mainFrame: 1,
      frames: [
        row(1, '9:1', 'https://one.test/top'),
        row(2, '9:2', 'https://one.test/child', 1),
        row(3, '9:3', 'https://other.test/child', 1)
      ]
    }
  };
  const native = {
    frames: {
      snapshot(id) {
        state.onSnapshot?.();
        if (state.failure) {
          throw state.failure;
        }
        return Object.hasOwn(state, 'raw')
          ? state.raw
          : JSON.stringify(
              id === 7 ? state.snapshot : { contentsId: id, mainFrame: null, frames: [] }
            );
      }
    }
  };
  const api = create(
    native,
    (id) => id === 7 && state.alive,
    error,
    unsupported,
    () => (state.alive ? [7] : [])
  );
  return { state, native, api };
}

const DESTROYED = 'ERR_FIREFLY_OBJECT_DESTROYED';
const DATA = 'ERR_FIREFLY_FRAME_DATA';

{
  const { api } = fixture();
  const main = api.main(7);
  check(
    main && main.url === 'https://one.test/top' && main.origin === 'https://one.test',
    'Native main frame before any IPC'
  );
  check(
    api.main(7) === main && api.fromDocument(7, '9:1') === main && api.fromId(24, 201) === main,
    'Canonical wrapper identity across lookup routes'
  );
  check(
    main.processId === 24 && main.osProcessId === 4242 && api.fromId(4242, 201) === undefined,
    'Internal process identity distinct semantically from OS pid'
  );
  check(main.parent === null && main.top === main && main.frames.length === 2, 'Native ancestry');
  for (const child of main.frames) {
    check(
      child.parent === main && child.top === main && !child.isDestroyed(),
      'Child canonical links'
    );
  }
  const copy = main.frames;
  copy.length = 0;
  check(main.frames.length === 2, 'Child arrays are owned snapshots');
  api.stop();
  console.log(
    'PASS native main frame before IPC canonical lookup identity process semantics and ancestry'
  );
}

{
  const { api, state } = fixture();
  const main = api.main(7);
  const old = main.frames[0];
  state.snapshot.frames[0].url += '\u0023same-document';
  check(
    api.main(7) === main && main.url.endsWith('#same-document'),
    'Same document URL mutation retains identity'
  );
  state.snapshot.frames[1] = {
    ...state.snapshot.frames[1],
    token: 4,
    document: '9:4',
    routingId: 204,
    url: 'https://one.test/replaced'
  };
  check(
    old.isDestroyed() &&
      api.fromDocument(7, '9:2') === undefined &&
      api.fromId(24, 202) === undefined,
    'Replaced active document retires old token'
  );

  fails(() => old.url, DESTROYED);

  fails(() => old.parent, DESTROYED);

  const fresh = main.frames.find((value) => value.url === 'https://one.test/replaced');
  check(fresh && fresh !== old && fresh.parent === main, 'Replacement gets new canonical wrapper');
  api.stop();
  console.log(
    'PASS same document URL retains wrapper cross-document replacement revokes stale properties'
  );
}

{
  const { api, state } = fixture();
  const main = api.main(7);
  const child = main.frames[0];
  let readOnly = false;
  try {
    main.url = 'https://forged.test';
  } catch {
    readOnly = true;
  }
  check(readOnly && main.url === 'https://one.test/top', 'Read-only frame URL');

  fails(() => main.executeJavaScript('1'), 'ERR_FIREFLY_UNSUPPORTED_API');

  state.alive = false;
  check(
    main.isDestroyed() && child.isDestroyed(),
    'Destroyed contents invalidate entire native tree'
  );

  fails(() => main.processId, DESTROYED);

  fails(() => child.top, DESTROYED);

  api.stop();
  console.log(
    'PASS read-only wrappers explicit unsupported operations and contents destruction invalidation'
  );
}

{
  for (const mutate of [
    (snapshot) => {
      snapshot.contentsId = 8;
    },
    (snapshot) => {
      snapshot.mainFrame = 999;
    },
    (snapshot) => {
      snapshot.frames[1].parent = 999;
    },
    (snapshot) => {
      snapshot.frames[1].top = 2;
    },
    (snapshot) => {
      snapshot.frames[2].token = 2;
    },
    (snapshot) => {
      snapshot.frames[0].parent = 2;
    },
    (snapshot) => {
      snapshot.frames[0].processId = '24';
    },
    (snapshot) => {
      snapshot.frames[0].osProcessId = NaN;
    },
    (snapshot) => {
      snapshot.frames[0].document = 'invalid';
    }
  ]) {
    const { api, state } = fixture();
    mutate(state.snapshot);

    fails(() => api.main(7), DATA);

    api.stop();
  }
  for (const raw of ['null', '[]', '{', {}, 5]) {
    const { api, state } = fixture();
    state.raw = raw;

    fails(() => api.main(7), DATA);

    api.stop();
  }
  console.log(
    'PASS malformed canonical native snapshots identity collisions ancestry cycles and scalar validation'
  );
}

{
  const { api, state, native } = fixture();
  check(
    api.fromDocument(7, '9:999') === undefined && api.fromId(999, 999) === undefined,
    'Unknown frame lookup'
  );
  state.snapshot = { contentsId: 7, mainFrame: null, frames: [] };
  check(
    api.main(7) === null || api.main(7) === undefined,
    'No invented main frame before native document exists'
  );
  delete native.frames.snapshot;

  fails(() => api.main(7), 'ERR_FIREFLY_UNSUPPORTED_API');

  api.stop();
  console.log('PASS unknown lookups empty native document and missing provider refusal');
}

{
  const { api, state } = fixture();
  const main = api.main(7);
  api.stop();
  api.stop();
  check(main.isDestroyed(), 'Stop permanently invalidates wrappers');

  fails(() => main.url, DESTROYED);

  const reentrant = fixture();
  const known = reentrant.api.main(7);

  reentrant.state.onSnapshot = () => {
    reentrant.state.alive = false;
  };

  check(known.isDestroyed(), 'Native snapshot reentry cannot retain stale live identity');

  fails(() => known.url, DESTROYED);

  reentrant.api.stop();
  console.log('PASS permanent shutdown and native snapshot lifecycle reentry fail closed');
}
console.log('DONE canonical frames policy');
