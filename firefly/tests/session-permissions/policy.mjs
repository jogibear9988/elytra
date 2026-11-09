/** Real session policy facade with controlled native documents; no renderer/OS permission grant. */
import create from '../../core/session/permissions.mjs';

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
  return caught;
}

function fixture() {
  const state = {
    ready: true,
    stopped: false,
    destroyed: false,
    configurations: [],
    completions: [],
    pending: new Map()
  };
  const contents = { id: 7, isDestroyed: () => state.destroyed };
  const native = {
    permissions: {
      configure(epoch, callback, enabled) {
        for (const [token, owner] of state.pending) {
          state.completions.push({ token, epoch: owner, allowed: false });
        }
        state.pending.clear();
        state.configurations.push({ epoch, callback, enabled });
        state.onConfigure?.();
        if (state.failure) {
          throw state.failure;
        }
      },

      complete(token, epoch, allowed) {
        if (state.pending.get(token) !== epoch) {
          return;
        }
        state.pending.delete(token);
        state.completions.push({ token, epoch, allowed });
        state.onComplete?.();
      }
    }
  };
  const component = create(
    native,
    () => state.ready,
    () => state.stopped,
    (id) => (id === 7 ? contents : undefined),
    error,
    unsupported
  );
  const details = {
    requestingUrl: 'http://request.test/page',
    isMainFrame: true,
    securityOrigin: 'http://request.test'
  };
  return {
    component,
    session: component.api,
    state,
    contents,
    native,
    details,

    config: () => state.configurations.at(-1),

    packet(token = 1, fields = {}) {
      const packet = {
        token,
        epoch: state.configurations.at(-1).epoch,
        contentsId: 7,
        permission: 'geolocation',
        ...details,
        ...fields
      };
      if (
        state.configurations.at(-1).enabled &&
        packet.epoch === state.configurations.at(-1).epoch &&
        Number.isInteger(token) &&
        token > 0
      ) {
        state.pending.set(token, packet.epoch);
      }
      return packet;
    }
  };
}

const TYPE = 'ERR_INVALID_ARG_TYPE';
const CLOSED = 'ERR_FIREFLY_SESSION_CLOSED';

{
  const f = fixture();
  f.state.ready = false;

  fails(() => f.session.setPermissionCheckHandler(null), 'ERR_FIREFLY_NOT_READY');

  fails(() => f.session.setPermissionRequestHandler(null), 'ERR_FIREFLY_NOT_READY');

  f.state.ready = true;
  for (const value of [undefined, 0, false, {}, 'handler']) {
    fails(() => f.session.setPermissionCheckHandler(value), TYPE);

    fails(() => f.session.setPermissionRequestHandler(value), TYPE);
  }
  check(f.session.setPermissionCheckHandler(null) === undefined, 'Null check reset');
  const first = f.config().epoch;
  check(
    f.config().callback === null && f.config().enabled === false,
    'Defaultdeny native configuration'
  );
  check(
    f.session.setPermissionRequestHandler(null) === undefined && f.config().epoch > first,
    'Monotonic replacement epochs'
  );
  f.component.stop();
  console.log('PASS readiness strict setter arguments null resets and monotonic native epochs');
}

{
  const f = fixture();
  const calls = [];

  f.session.setPermissionCheckHandler((...args) => {
    calls.push(args);
    return false;
  });

  const first = f.config();
  check(
    first.callback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)) === false,
    'Denied genuine native check'
  );
  check(
    calls.length === 1 &&
      calls[0][0] === f.contents &&
      calls[0][2] === 'http://request.test' &&
      calls[0][3].requestingUrl === f.details.requestingUrl,
    'Authoritative contents origin and frame details'
  );

  f.session.setPermissionCheckHandler(() => true);

  check(
    first.callback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)) === false,
    'Stale check callback cannot retain grant'
  );
  check(
    f.config().callback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)) ===
      true,
    'Supported query grant only'
  );
  for (const [id, permission, details] of [
    [999, 'geolocation', f.details],
    [7, 'unknown', f.details],
    [7, 'geolocation', {}]
  ]) {
    check(
      f.config().callback(id, permission, 'http://request.test', JSON.stringify(details)) === false,
      'Malformed or unknown native check fails closed'
    );
  }
  f.state.destroyed = true;
  check(
    f.config().callback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)) ===
      false,
    'Destroyed contents cannot authorize'
  );
  f.component.stop();
  console.log(
    'PASS synchronous check binding malformed data stale replacement and live contents ownership'
  );
}

{
  const f = fixture();
  let saved;
  let calls = 0;

  f.session.setPermissionRequestHandler((contents, permission, callback, details) => {
    calls++;
    saved = callback;
    check(
      contents === f.contents && permission === 'geolocation' && details.isMainFrame,
      'Actual request metadata'
    );
    callback(false);
  });

  const packet = f.packet();
  f.component.dispatch(packet);
  check(
    calls === 1 && f.state.completions.length === 1 && f.state.completions[0].allowed === false,
    'Request denial native settlement'
  );
  saved(true);
  check(
    calls === 1 && f.state.completions.length === 1,
    'Duplicate decision callback cannot settle again'
  );

  f.session.setPermissionRequestHandler(() => {
    throw error('OWN_PERMISSION_THROW', 'User callback failed');
  });

  fails(() => f.component.dispatch(f.packet(2)), 'OWN_PERMISSION_THROW');

  check(
    f.state.completions.at(-1).allowed === false,
    'Throwing request handler denies before error propagation'
  );

  f.session.setPermissionRequestHandler((_contents, _permission, callback) => callback('yes'));

  fails(() => f.component.dispatch(f.packet(3)), TYPE);

  check(
    f.state.completions.at(-1).allowed === false,
    'Nonboolean callback decision denied before validation error'
  );

  f.session.setPermissionRequestHandler((_contents, _permission, callback) => callback(true));

  fails(
    () => f.component.dispatch(f.packet(4, { permission: 'media', mediaTypes: ['audio'] })),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );

  check(
    f.state.completions.at(-1).allowed === false,
    'Unsupported media grant never reaches native true'
  );
  f.component.stop();
  console.log(
    'PASS request callback denial exactly-once ownership throw cleanup and unsupported media grant'
  );
}

{
  const f = fixture();
  let oldCallback;
  let calls = 0;

  f.session.setPermissionRequestHandler((_contents, _permission, callback) => {
    oldCallback = callback;
    calls++;
  });

  const packet = f.packet();
  f.component.dispatch(packet);

  f.session.setPermissionRequestHandler(() => {
    calls++;
  });

  check(
    f.state.completions.length === 1 && !f.state.completions[0].allowed,
    'Replacement cancels pending old-epoch request'
  );
  oldCallback(true);
  f.component.dispatch(packet);
  check(
    calls === 1 && f.state.completions.length === 1,
    'Old decision cannot settle after replacement'
  );

  f.session.setPermissionRequestHandler((_contents, _permission, callback) => {
    f.session.setPermissionRequestHandler(null);
    callback(true);
  });

  f.component.dispatch(f.packet(2));
  check(
    f.state.completions.at(-1).allowed === false,
    'Reentrant reset revokes currently executing callback'
  );

  f.session.setPermissionCheckHandler(() => {
    f.session.setPermissionCheckHandler(null);
    return true;
  });

  const checkCallback = f.config().callback;
  check(
    checkCallback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)) === false,
    'Reentrant check replacement cannot return stale grant'
  );
  f.component.stop();
  console.log(
    'PASS pending epoch replacement reset and reentrant handler mutation revoke stale decisions'
  );
}

{
  const f = fixture();
  let calls = 0;

  f.session.setPermissionRequestHandler(() => {
    calls++;
  });

  for (const [token, fields] of [
    [1, { contentsId: 999 }],
    [2, { permission: 'unsupported' }],
    [3, { requestingUrl: 3 }],
    [4, { isMainFrame: 'true' }]
  ]) {
    f.component.dispatch(f.packet(token, fields));
  }
  check(
    calls === 0 &&
      f.state.completions.length === 4 &&
      f.state.completions.every((item) => item.allowed === false),
    'Malformed native requests deny without application callbacks'
  );
  f.component.dispatch({ token: 0, epoch: f.config().epoch });
  f.component.dispatch({ token: '1', epoch: f.config().epoch });
  check(f.state.completions.length === 4, 'Invalid native identities cannot target settlement');

  f.session.setPermissionCheckHandler(() => {
    throw error('OWN_CHECK_THROW', 'Check failed');
  });

  fails(
    () => f.config().callback(7, 'geolocation', 'http://request.test', JSON.stringify(f.details)),
    'OWN_CHECK_THROW'
  );

  f.component.stop();
  console.log(
    'PASS malformed event identities metadata unknown contents fail closed and check errors propagate'
  );
}

{
  const f = fixture();
  let callback;

  f.session.setPermissionRequestHandler((_contents, _permission, reply) => {
    callback = reply;
  });

  f.component.dispatch(f.packet());
  f.component.stop();
  f.component.stop();
  check(
    f.state.completions.length === 1 && !f.state.completions[0].allowed,
    'Stop retires all pending permission decisions'
  );
  callback(true);
  f.component.dispatch(f.packet(2));
  check(
    f.state.completions.length === 1 && f.config().callback === null && !f.config().enabled,
    'No late application decision after permanent shutdown'
  );

  fails(() => f.session.setPermissionCheckHandler(() => true), CLOSED);

  fails(() => f.session.setPermissionRequestHandler(null), CLOSED);

  const g = fixture();

  g.state.onConfigure = () => {
    g.state.stopped = true;
    g.component.stop();
  };

  fails(() => g.session.setPermissionCheckHandler(() => true), CLOSED);

  check(
    g.config().callback === null && !g.config().enabled,
    'Native configure exit reentry cannot preserve enabled handler'
  );
  const h = fixture();

  h.state.onConfigure = () =>
    fails(() => h.session.setPermissionRequestHandler(null), 'ERR_FIREFLY_SESSION_REENTRANT');

  h.session.setPermissionCheckHandler(() => true);

  check(h.state.configurations.length === 1, 'Nested setter cannot publish another native epoch');
  h.state.onConfigure = undefined;
  h.component.stop();
  const failed = fixture();
  failed.state.failure = error('OWN_CONFIGURE_FAILURE', 'Native configure failed');
  check(
    fails(() => failed.session.setPermissionCheckHandler(() => true), 'OWN_CONFIGURE_FAILURE') ===
      failed.state.failure,
    'Native configure error preserved'
  );
  check(
    failed
      .config()
      .callback(7, 'geolocation', 'http://request.test', JSON.stringify(failed.details)) === false,
    'Failed candidate check remains revoked'
  );
  delete failed.state.failure;
  failed.component.stop();
  console.log('PASS pending shutdown stale callbacks closed setters and native configure reentry');
}
console.log('DONE session permissions policy');
