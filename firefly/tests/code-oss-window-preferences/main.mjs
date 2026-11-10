/** Real Node/Mite helper policy plus exact pinned Code-OSS preference-expression evaluation. */
import assert from 'node:assert/strict';
import { libJSWindowCachePreferences, libJSWindowPastePreferences } from './window-preferences.mjs';
import {
  originalPreferences,
  adaptedPreferences,
  originalPastePreferences,
  adaptedPastePreferences
} from './expressions.mjs';

const fixture = JSON.parse(process.argv[2]);
const mite =
  typeof process.versions.mite === 'string' &&
  process.versions.node === undefined &&
  process.versions.electron === undefined;
const local = {
  enabled: true,
  scope: 'experimental-local-desktop',
  disabled: ['webviews', 'remote', 'display-capture', 'engine-code-cache']
};
const engine = { engine: 'LibJS/LibWeb', scope: 'implicit-code-oss-engine-tuning-only' };
if (fixture.policy !== 'missing-local') {
  process._vscodeLibJSLocalDesktopStatus = local;
}
if (fixture.policy !== 'missing-engine') {
  process._vscodeLibJSEngineStatus = engine;
}
if (fixture.policy === 'disabled-local') {
  local.enabled = false;
}
if (fixture.policy === 'wrong-local-scope') {
  local.scope = 'wrong';
}
if (fixture.policy === 'cache-not-disabled') {
  local.disabled = ['webviews'];
}
if (fixture.policy === 'wrong-engine') {
  engine.engine = 'V8';
}
if (fixture.policy === 'wrong-engine-scope') {
  engine.scope = 'wrong';
}
if (fixture.policy === 'conflict') {
  process._vscodeLibJSWindowPreferencesStatus = { unexpected: 'existing status' };
}
if (fixture.policy === 'preexisting-matching') {
  process._vscodeLibJSWindowPreferencesStatus = {
    engine: 'LibJS/LibWeb',
    scope: 'code-oss-disabled-v8-window-cache-only',
    cache: 'unavailable',
    omittedWindowPreferences: [
      { name: 'v8CacheOptions', value: 'none', reason: 'existing untrusted status' }
    ]
  };
}
const beforeStatus = process._vscodeLibJSWindowPreferencesStatus;

function context(useCodeCache, sessions) {
  const calls = [];
  const service = {
    configObjectUrl: {
      resource: {
        toString() {
          calls.push('config-url');
          return 'vscode:owned-config';
        }
      }
    },
    environmentMainService: {
      get useCodeCache() {
        calls.push('use-code-cache');
        return useCodeCache;
      }
    }
  };
  const access = {
    asFileUri(value) {
      calls.push('preload-path:' + value);
      return {
        get fsPath() {
          calls.push('preload-fs-path');
          return '/owned/preload.js';
        }
      };
    }
  };
  return {
    calls,
    service,
    access,
    config: {
      get isSessionsWindow() {
        calls.push('sessions');
        return sessions;
      }
    }
  };
}

const source = context(fixture.useCodeCache, fixture.sessions);
const baseline = originalPreferences.call(source.service, source.access, source.config);
const actualContext = context(fixture.useCodeCache, fixture.sessions);
let actual;
let failure;
try {
  actual = adaptedPreferences.call(
    actualContext.service,
    actualContext.access,
    actualContext.config
  );
} catch (error) {
  failure = error;
}
assert.deepEqual(
  actualContext.calls,
  failure ? source.calls.slice(0, source.calls.indexOf('use-code-cache') + 1) : source.calls,
  'Pinned source preserves accessor order and failure stops at the explicitly unsupported preference'
);
assert.equal(source.calls.filter((entry) => entry === 'use-code-cache').length, 1);
const allowed =
  fixture.policy === 'valid' &&
  process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP === '1' &&
  !process.env.VSCODE_CODE_CACHE_PATH &&
  !fixture.useCodeCache;
if (!mite || allowed) {
  assert.equal(failure, undefined);
  if (mite) {
    assert.equal(Object.hasOwn(actual, 'v8CacheOptions'), false);
    const expected = { ...baseline };
    delete expected.v8CacheOptions;
    assert.deepEqual(actual, expected);
    const published = process._vscodeLibJSWindowPreferencesStatus;
    assert.equal(published.engine, 'LibJS/LibWeb');
    assert.equal(published.scope, 'code-oss-disabled-v8-window-cache-only');
    assert.equal(published.cache, 'unavailable');
    assert.equal(published.omittedWindowPreferences.length, 1);
    assert.equal(published.omittedWindowPreferences[0].name, 'v8CacheOptions');
    assert.equal(published.omittedWindowPreferences[0].value, 'none');
    assert.equal(typeof published.omittedWindowPreferences[0].reason, 'string');
    assert.ok(published.omittedWindowPreferences[0].reason.includes('V8'));
    assert.ok(
      Object.isFrozen(published) &&
        Object.isFrozen(published.omittedWindowPreferences) &&
        Object.isFrozen(published.omittedWindowPreferences[0])
    );
    const descriptor = Object.getOwnPropertyDescriptor(
      process,
      '_vscodeLibJSWindowPreferencesStatus'
    );
    assert.equal(descriptor.writable, false);
    assert.equal(descriptor.configurable, false);
    assert.equal(descriptor.enumerable, false);
    assert.deepEqual(libJSWindowCachePreferences('none'), {});
    assert.equal(
      process._vscodeLibJSWindowPreferencesStatus,
      published,
      'Repeated windows reuse the diagnostic'
    );

    assert.throws(
      () => libJSWindowCachePreferences('bypassHeatCheck'),
      (error) => error.code === 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    );

    local.enabled = false;

    assert.throws(
      () => libJSWindowCachePreferences('none'),
      (error) => error.code === 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    );

    assert.equal(process._vscodeLibJSWindowPreferencesStatus, published);
  } else {
    assert.deepEqual(actual, baseline, 'Actual Node preserves original Electron/V8 values');
    assert.equal(process._vscodeLibJSWindowPreferencesStatus, beforeStatus);
    for (const value of ['none', 'bypassHeatCheck']) {
      assert.deepEqual(libJSWindowCachePreferences(value), { v8CacheOptions: value });
    }
  }
} else {
  assert.equal(failure?.code, 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION');
  assert.equal(
    process._vscodeLibJSWindowPreferencesStatus,
    beforeStatus,
    'Failed admission does not publish or replace diagnostics'
  );
}
// This independent object-spread boundary proves other observable/security fields
// and object identities survive; the pinned expression above supplies source proof.
if (!mite || allowed) {
  if (mite) {
    local.enabled = true;
  }
  const token = {};
  const order = [];
  const preferences = {
    get sandbox() {
      order.push('sandbox');
      return true;
    },

    get contextIsolation() {
      order.push('contextIsolation');
      return true;
    },

    get nodeIntegration() {
      order.push('nodeIntegration');
      return false;
    },

    get preload() {
      order.push('preload');
      return token;
    }
  };
  const result = { ...preferences, ...libJSWindowCachePreferences('none') };
  assert.deepEqual(order, ['sandbox', 'contextIsolation', 'nodeIntegration', 'preload']);
  assert.equal(result.sandbox, true);
  assert.equal(result.contextIsolation, true);
  assert.equal(result.nodeIntegration, false);
  assert.equal(result.preload, token);
}
// Exact pinned default-window preference expression: keep native identities,
// accessor order and every preference except the explicit legacy script-paste flag.
const pasteArgs = [{ secureToken: {} }, { zoomLevel: 2 }, {}, (value) => value * 10];
const pasteBaseline = originalPastePreferences(...pasteArgs);
const pasteAllowed =
  ['valid', 'conflict', 'preexisting-matching'].includes(fixture.policy) &&
  process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP === '1' &&
  !process.env.VSCODE_CODE_CACHE_PATH;
if (!mite || pasteAllowed) {
  const pasteActual = adaptedPastePreferences(...pasteArgs);
  const expected = { ...pasteBaseline };
  if (mite) {
    delete expected.enableDeprecatedPaste;
    const status = process._vscodeLibJSLegacyPasteStatus;
    assert.equal(status.scope, 'code-oss-deprecated-sync-paste-omission-only');
    assert.equal(status.omittedWindowPreferences.length, 1);
    assert.equal(status.omittedWindowPreferences[0].name, 'enableDeprecatedPaste');
    assert.equal(status.omittedWindowPreferences[0].value, true);
    assert.ok(status.omittedWindowPreferences[0].reason.includes('native webContents.paste'));
    assert.ok(
      Object.isFrozen(status) &&
        Object.isFrozen(status.omittedWindowPreferences) &&
        Object.isFrozen(status.omittedWindowPreferences[0])
    );
    assert.deepEqual(libJSWindowPastePreferences(true), {});
    assert.equal(process._vscodeLibJSLegacyPasteStatus, status);

    assert.throws(
      () => libJSWindowPastePreferences(false),
      (error) => error.code === 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    );

    local.enabled = false;

    assert.throws(
      () => libJSWindowPastePreferences(true),
      (error) => error.code === 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
    );

    local.enabled = true;
  } else {
    assert.equal(process._vscodeLibJSLegacyPasteStatus, undefined);
    assert.deepEqual(libJSWindowPastePreferences(false), { enableDeprecatedPaste: false });
  }
  assert.deepEqual(pasteActual, expected);
  assert.equal(pasteActual.secureToken, pasteArgs[0].secureToken);
} else {
  assert.throws(
    () => adaptedPastePreferences(...pasteArgs),
    (error) => error.code === 'ERR_CODE_OSS_LIBJS_ENGINE_OPTION'
  );

  assert.equal(process._vscodeLibJSLegacyPasteStatus, undefined);
}
console.log(
  JSON.stringify({
    runtime: mite ? 'Mite' : 'Node',
    accepted: !failure,
    omitted: actual ? !Object.hasOwn(actual, 'v8CacheOptions') : false,
    calls: actualContext.calls,
    diagnostic: process._vscodeLibJSWindowPreferencesStatus
  })
);
