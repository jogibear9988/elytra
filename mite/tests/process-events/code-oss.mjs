/** Execute the actual type-erased Code-OSS module, then its portable source-mode path. */
import path from 'node:path';
import { pathToFileURL } from 'node:url';
const originalCwd = process.cwd();
const source = process.argv[2];
const portablePath = process.argv[3];
const bootstrap = await import(pathToFileURL(source).href);

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

check(process.env.VSCODE_CWD === originalCwd, 'Code-OSS captures original cwd');
check(
  process.cwd() === (process.platform === 'win32' ? path.dirname(process.execPath) : originalCwd),
  'Code-OSS platform cwd setup'
);
check(process.listenerCount('SIGPIPE') === 1, 'Actual Code-OSS SIGPIPE registration');
check(typeof bootstrap.devInjectNodeModuleLookupPath === 'function', 'Named exports');
process.env.VSCODE_PORTABLE = portablePath;
const portable = bootstrap.configurePortable({ applicationName: 'code-oss' });
check(portable.isPortable && portable.portableDataPath === portablePath, 'Actual portable mode');
const temp = path.join(portablePath, 'tmp');
if (process.platform === 'win32') {
  check(process.env.TEMP === temp && process.env.TMP === temp, 'Portable Windows temp');
} else {
  check(process.env.TMPDIR === temp, 'Portable POSIX temp');
}
process.env.VSCODE_PORTABLE = path.join(portablePath, 'absent');
const absent = bootstrap.configurePortable({ applicationName: 'code-oss' });
check(!absent.isPortable && process.env.VSCODE_PORTABLE === undefined, 'Absent portable mode');
process.removeAllListeners('SIGPIPE');
console.log('PASS actual pinned Code-OSS source bootstrap SIGPIPE/cwd/portable paths');
