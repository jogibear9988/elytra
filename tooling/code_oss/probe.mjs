/**
 * Run the real compiled entry and preserve failure as a nonzero result. The
 * diagnostic records how far initialization got; it is never a desktop pass.
 * Use standalone Mite for a non-GUI trace of the first missing host dependency.
 */
import path from 'node:path';
import { pathToFileURL } from 'node:url';
const tree = path.resolve(process.argv[2]);
try {
  await import(pathToFileURL(path.join(tree, 'src/main.js')).href);
  console.log(
    JSON.stringify({
      entryCompleted: true,
      cache: process._vscodeNodeCompileCacheStatus,
      engine: process._vscodeLibJSEngineStatus,
      localDesktop: process._vscodeLibJSLocalDesktopStatus
    })
  );
} catch (error) {
  console.error(
    JSON.stringify({
      entryCompleted: false,
      error: { name: error.name, code: error.code, message: error.message },
      cache: process._vscodeNodeCompileCacheStatus,
      engine: process._vscodeLibJSEngineStatus,
      localDesktop: process._vscodeLibJSLocalDesktopStatus
    })
  );
  process.exitCode = 1;
}
if (process.argv[3] === '--firefly') {
  // Explicit diagnostic shutdown: no successful desktop launch is inferred.
  const { app } = await import('firefly');
  app.exit(Number(process.exitCode ?? 0));
}
if (process.argv[3] === '--firefly-ready') {
  // Let the real ready callback and asynchronous module graph run. This bounded
  // observation deliberately cannot report workbench or desktop acceptance.
  const { app } = await import('firefly');

  app
    .whenReady()
    .then(() => console.log(JSON.stringify({ appReadyObserved: true, workbenchAccepted: false })));

  setTimeout(() => {
    console.log(JSON.stringify({ observationEnded: true, workbenchAccepted: false }));
    app.exit(1);
  }, 3000);
}
