/** Exercise actual adapted upstream cache control flow, not an imitation bootstrap. */
import { pathToFileURL } from 'node:url';
import fs from 'node:fs';
import path from 'node:path';
const root = process.argv[2];
const mode = process.argv[3];
const cache = await import(
  pathToFileURL(path.join(root, 'src/vs/base/node/nodeCompileCache.js')).href
);

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function refused(callback, code) {
  try {
    callback();
  } catch (error) {
    if (error.code === code) {
      return;
    }
    throw error;
  }
  throw new Error(`Expected ${code}`);
}

if (process.versions.mite !== undefined) {
  const runtimeModules = await import('node:module');
  check(!runtimeModules.isBuiltin('node:v8'), 'No fabricated V8 builtin');
  check(!('enableCompileCache' in runtimeModules), 'Application policy is not a fake Node API');
}

if (['generation', 'measurements'].includes(mode)) {
  const code = `ERR_CODE_OSS_LIBJS_CACHE_${mode.toUpperCase()}`;

  refused(() => cache.enableNodeCompileCache('main'), code);

  refused(() => cache.markNodeCompileCacheReady(), code);

  check(
    !fs.existsSync(process.env.VSCODE_NODE_COMPILE_CACHE_ROOT),
    'Unsupported modes touched disk'
  );
  console.log(`PASS Code-OSS cache ${mode} explicitly refused before disk changes`);
} else {
  if (mode === 'disabled') {
    const directory = path.join(process.env.VSCODE_NODE_COMPILE_CACHE_ROOT, 'main');
    fs.mkdirSync(directory, { recursive: true });
    fs.writeFileSync(
      path.join(directory, `${process.version}-${process.arch}-fixture`),
      'Not bytecode'
    );
  }
  check(cache.enableNodeCompileCache('main') === false, 'No bytecode cache generation claim');
  const status = process._vscodeNodeCompileCacheStatus;
  check(status.status === mode, 'Actual upstream status branch');
  check(
    !status.isPackagedCacheEnabled && status.activeDirectory === undefined,
    'No active V8 cache'
  );
  check(process.env.VSCODE_NODE_COMPILE_CACHE_KIND === undefined, 'No fake active cache kind');
  check(
    status.configuredDirectory === path.join(process.env.VSCODE_NODE_COMPILE_CACHE_ROOT, 'main'),
    'Real configured path'
  );
  const logs = [];

  cache.logNodeCompileCacheStatus((message) => logs.push(message));

  cache.markNodeCompileCacheReady((message) => logs.push(message));

  check(
    logs.length === 1 && logs[0].includes(`status: ${mode}`),
    'Actual once-only readiness logging'
  );
  check(
    !fs.existsSync(path.join(status.configuredDirectory, '.ready')),
    'No fabricated readiness marker'
  );
  check(
    cache.getNodeCompileCacheKindForUtilityProcess('extensionHost') === 'extension-host',
    'Utility kind mapping'
  );
  check(
    cache.getNodeCompileCacheKindForUtilityProcess('ptyHost') === 'pty-host',
    'PTY kind mapping'
  );
  if (mode === 'disabled') {
    const policy = await import(
      pathToFileURL(path.join(root, 'src/vs/base/node/elytraCompileCache.js')).href
    );
    check(
      status.message.includes('LibJS') && policy.getCompileCacheDir() === undefined,
      'Truthful disabled reason'
    );

    refused(() => policy.flushCompileCache(), 'ERR_CODE_OSS_LIBJS_CACHE_GENERATION');

    refused(() => policy.getHeapCodeStatistics(), 'ERR_CODE_OSS_LIBJS_V8_STATISTICS');

    try {
      await cache.waitForNodeCompileCacheReady();
      throw new Error('Unexpected cache wait');
    } catch (error) {
      check(error.code === 'ERR_CODE_OSS_LIBJS_CACHE_WAIT', 'Explicit unsupported wait');
    }
  }
  console.log(`PASS actual Code-OSS cache ${mode} branch/readiness/kind mapping`);
}
