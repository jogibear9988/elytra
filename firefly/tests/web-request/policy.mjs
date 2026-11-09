/** Actual session interception policy with controlled native metadata; no endpoint traffic. */
import create from '../../core/session/web-request.mjs';

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

async function rejects(promise, code) {
  let caught;
  try {
    await promise;
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected rejection ' + code + ', got ' + caught?.code);
}

function fixture() {
  const state = {
    ready: true,
    stopped: false,
    dead: false,
    frameDead: false,
    configurations: [],
    completions: []
  };
  const contents = { id: 7, isDestroyed: () => state.dead };
  const frame = {
    url: 'https://owner.test/page',
    processId: 22,
    parent: null,

    isDestroyed: () => state.frameDead
  };
  const native = {
    webRequest: {
      configure(epoch, before, headers) {
        state.configurations.push({ epoch, before, headers });
        state.onConfigure?.();
        if (state.failure) {
          throw state.failure;
        }
      },

      complete(token, epoch, json) {
        state.completions.push({ token, epoch, value: JSON.parse(json) });
      }
    }
  };
  const abortHooks = {
    subscribe(signal, callback) {
      if (signal.aborted) {
        callback();
      } else {
        signal.addEventListener('abort', callback, { once: true });
      }
      return () => signal.removeEventListener('abort', callback);
    }
  };
  const component = create(
    native,
    () => state.ready,
    () => state.stopped,
    (id) => (id === 7 ? contents : undefined),
    (id, doc) => (id === 7 && doc === '9:1' ? frame : undefined),
    error,
    unsupported,
    abortHooks
  );

  const data = (fields = {}) => ({
    id: 3,
    url: 'https://target.test/data',
    method: 'GET',
    resourceType: 'xhr',
    referrer: 'https://owner.test/page',
    contentsId: 7,
    document: '9:1',
    ...fields
  });

  const packet = (fields = {}) => ({
    token: 1,
    epoch: state.configurations.at(-1).epoch,
    stage: 'before',
    ...data(),
    ...fields
  });

  return { state, contents, frame, native, component, api: component.api, data, packet };
}

const BLOCKED = 'ERR_BLOCKED_BY_CLIENT';

{
  const f = fixture();
  f.state.ready = false;

  fails(() => f.api.onBeforeRequest(null), 'ERR_FIREFLY_NOT_READY');

  f.state.ready = true;
  for (const args of [[], [{}], [false], [{}, null, null]]) {
    fails(() => f.api.onBeforeRequest(...args), 'ERR_INVALID_ARG_TYPE');
  }

  fails(() => f.api.onBeforeRequest({}, () => {}), 'ERR_INVALID_ARG_VALUE');

  fails(
    () => f.api.onBeforeRequest({ urls: ['not a pattern'] }, () => {}),
    'ERR_INVALID_ARG_VALUE'
  );

  fails(
    () => f.api.onBeforeRequest({ urls: ['<all_urls>'], extra: false }, () => {}),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );

  fails(() => f.api.onBeforeSendHeaders(() => {}), 'ERR_FIREFLY_UNSUPPORTED_API');

  let old = 0;
  let current = 0;

  f.api.onBeforeRequest((_details, callback) => {
    old++;
    callback({ cancel: true });
  });

  f.api.onBeforeRequest((_details, callback) => {
    current++;
    callback({ cancel: false });
  });

  check(
    (await f.component.intercept('before', f.data())).cancel === false &&
      old === 0 &&
      current === 1,
    'Only last listener runs'
  );
  f.api.onBeforeRequest(null);
  await f.component.intercept('before', f.data());
  check(current === 1, 'Null removeslistener');
  const ids = [f.component.allocateId(), f.component.allocateId()];
  check(
    ids[0] > 0 && ids[0] % 2 === 0 && ids[1] === ids[0] + 2,
    'Independent bounded main request IDs'
  );
  f.component.stop();
  console.log(
    'PASS readiness registration validation last listener null reset and main request identity'
  );
}

{
  const f = fixture();
  let matched = 0;

  const install = (filter) =>
    f.api.onBeforeRequest(filter, (_details, callback) => {
      matched++;
      callback({ cancel: true });
    });

  install({
    urls: ['https://*.target.test:443/data*'],
    excludeUrls: ['*://target.test/data/excluded*'],
    types: ['xhr']
  });
  await rejects(f.component.intercept('before', f.data()), BLOCKED);
  await rejects(
    f.component.intercept('before', f.data({ url: 'https://sub.target.test/data?a=1' })),
    BLOCKED
  );
  for (const fields of [
    { url: 'http://target.test/data' },
    { url: 'https://target.test.evil/data' },
    { url: 'https://target.test/data/excluded?a=1' },
    { resourceType: 'image' }
  ]) {
    await f.component.intercept('before', f.data(fields));
  }
  check(matched === 2, 'Scheme host subdomain port URL query exclusions and resource type combine');
  install({ urls: [], types: [] });
  await rejects(f.component.intercept('before', f.data()), BLOCKED);
  install({ urls: ['file:///*'] });
  await rejects(f.component.intercept('before', f.data({ url: 'file:///owned/data' })), BLOCKED);
  install({ urls: ['app://bundle/*'] });
  await rejects(f.component.intercept('before', f.data({ url: 'app://bundle/data' })), BLOCKED);
  install({ urls: ['http://[::1]:*/data'] });
  await rejects(
    f.component.intercept('before', f.data({ url: 'http://[::1]:8123/data' })),
    BLOCKED
  );
  f.component.stop();
  console.log(
    'PASS native-style include exclude wildcard custom file IPv6 and empty resource filter semantics'
  );
}

{
  const f = fixture();
  let observed;

  f.api.onBeforeRequest((details, callback) => {
    observed = details;
    callback({ cancel: false });
  });

  f.component.dispatch(f.packet());
  check(
    observed.webContents === f.contents &&
      observed.webContentsId === 7 &&
      observed.frame === f.frame &&
      Number.isFinite(observed.timestamp),
    'Native canonical owner frame details'
  );
  const before = f.state.completions.length;
  for (const fields of [
    { contentsId: 999 },
    { document: 'unknown' },
    { resourceType: 'invalid' },
    { method: 'bad method' },
    { id: '3' }
  ]) {
    f.component.dispatch(f.packet(fields));
  }
  check(
    f.state.completions.slice(before).every((value) => value.value.cancel === true),
    'Malformed native metadata failsclosed'
  );
  f.state.frameDead = true;
  f.component.dispatch(f.packet({ token: 2 }));
  check(f.state.completions.at(-1).value.cancel, 'Dead frame cannot reachlistener');
  f.component.stop();
  console.log(
    'PASS native request metadata real owner frame resolution malformed identity and dead-frame denial'
  );
}

{
  const f = fixture();
  let saved;

  f.api.onBeforeRequest((_details, callback) => {
    saved = callback;
  });

  f.component.dispatch(f.packet());
  saved({ cancel: false });
  saved({ cancel: true });
  check(
    f.state.completions.length === 1 && !f.state.completions[0].value.cancel,
    'Duplicate decision cannot settle twice'
  );

  f.api.onBeforeRequest((_details, callback) => callback({ cancel: 'yes' }));

  fails(() => f.component.dispatch(f.packet({ token: 2 })), 'ERR_INVALID_ARG_TYPE');

  check(f.state.completions.at(-1).value.cancel, 'Malformed decision canceledbeforethrow');

  f.api.onBeforeRequest((_details, callback) => callback({ redirectURL: 'https://other.test' }));

  fails(() => f.component.dispatch(f.packet({ token: 3 })), 'ERR_FIREFLY_UNSUPPORTED_API');

  f.api.onBeforeRequest(() => {
    throw error('OWN_REQUEST_THROW', 'Listener failed');
  });

  fails(() => f.component.dispatch(f.packet({ token: 4 })), 'OWN_REQUEST_THROW');

  check(f.state.completions.at(-1).value.cancel, 'User exception settlescancelbeforepropagation');
  f.component.stop();
  console.log(
    'PASS duplicate decisions strict cancel validation unsupported redirects and callback error cleanup'
  );
}

{
  const f = fixture();
  let supplied;

  f.api.onHeadersReceived((details, callback) => {
    check(
      details.statusCode === 200 && details.responseHeaders['Content-Type'][0] === 'text/plain',
      'Real header-shaped metadata'
    );
    supplied = {
      'Content-Type': ['image/svg+xml'],
      'Access-Control-Allow-Origin': '*',
      'X-Byte': ['\x80\xff']
    };
    callback({ responseHeaders: supplied });
  });

  const result = await f.component.intercept(
    'headers',
    f.data({
      statusCode: 200,
      statusLine: 'HTTP/1.1 200 OK',
      responseHeaders: { 'Content-Type': ['text/plain'] }
    })
  );
  supplied['Content-Type'][0] = 'forged';
  check(
    result.responseHeaders['Content-Type'][0] === 'image/svg+xml' &&
      result.responseHeaders['Access-Control-Allow-Origin'][0] === '*',
    'Owned case-preserving header arrays'
  );
  for (const [responseHeaders, code] of [
    [{ 'bad name': 'value' }, 'ERR_INVALID_ARG_VALUE'],
    [{ Good: ['line\r\nbreak'] }, 'ERR_INVALID_ARG_VALUE'],
    [{ Good: ['one'], good: ['two'] }, 'ERR_INVALID_ARG_VALUE'],
    [{ Good: ['x'.repeat(32769)] }, 'ERR_OUT_OF_RANGE']
  ]) {
    f.api.onHeadersReceived((_details, callback) => callback({ responseHeaders }));

    fails(
      () =>
        f.component.dispatch(
          f.packet({
            stage: 'headers',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            responseHeaders: {}
          })
        ),
      code
    );

    check(
      f.state.completions.at(-1).value.cancel,
      'Invalid response header canceled before error propagation'
    );
  }
  f.component.stop();
  console.log(
    'PASS response header metadata status MIME CORS Latin1 snapshots and invalid header rejection'
  );
}

{
  const f = fixture();
  let saved;

  f.api.onBeforeRequest((_details, callback) => {
    saved = callback;
  });

  const waiting = rejects(f.component.intercept('before', f.data()), BLOCKED);
  f.api.onBeforeRequest(null);
  await waiting;
  saved({ cancel: false });

  f.api.onBeforeRequest((_details, callback) => {
    saved = callback;
  });

  const controller = new AbortController();
  const aborted = rejects(f.component.intercept('before', f.data(), controller.signal), BLOCKED);
  controller.abort();
  await aborted;
  saved({ cancel: false });

  f.api.onBeforeRequest((_details, callback) =>
    callback({
      get cancel() {
        f.api.onBeforeRequest(null);
        return false;
      }
    })
  );

  await rejects(f.component.intercept('before', f.data()), BLOCKED);

  f.api.onBeforeRequest((_details, callback) => {
    saved = callback;
  });

  const stopped = rejects(f.component.intercept('before', f.data()), BLOCKED);
  f.component.stop();
  await stopped;
  saved({ cancel: false });

  fails(() => f.api.onBeforeRequest(null), 'ERR_FIREFLY_SESSION_CLOSED');

  const quota = fixture();

  quota.api.onBeforeRequest(() => {});

  const quotaWaiting = Array.from({ length: 64 }, () =>
    rejects(quota.component.intercept('before', quota.data()), BLOCKED)
  );
  await rejects(quota.component.intercept('before', quota.data()), BLOCKED);
  quota.api.onBeforeRequest(null);
  await Promise.all(quotaWaiting);
  quota.component.stop();
  const configuring = fixture();

  configuring.state.onConfigure = () => configuring.component.stop();

  fails(
    () => configuring.api.onBeforeRequest((_details, callback) => callback({ cancel: false })),
    'ERR_FIREFLY_SESSION_CLOSED'
  );

  console.log(
    'PASS replacement abort decision-getter reentry shutdown and stale continuation revocation'
  );
}
console.log('DONE webRequest policy');
