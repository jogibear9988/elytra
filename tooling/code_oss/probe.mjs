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
    JSON.stringify({ entryCompleted: true, cache: process._vscodeNodeCompileCacheStatus })
  );
} catch (error) {
  console.error(
    JSON.stringify({
      entryCompleted: false,
      error: { name: error.name, code: error.code, message: error.message },
      cache: process._vscodeNodeCompileCacheStatus
    })
  );
  process.exitCode = 1;
}
