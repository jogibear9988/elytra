/** Probe-owned Code-OSS settings; never edit a caller's existing user profile. */
import { mkdirSync, writeFileSync } from 'node:fs';
import { randomUUID } from 'node:crypto';
import { tmpdir } from 'node:os';
import path from 'node:path';

export function prepareProbeProfile(argv) {
  const terminator = argv.indexOf('--', 3);
  const options = argv.slice(3, terminator === -1 ? argv.length : terminator);
  const count = options.filter((value) => value === '--native-title-bar').length;
  if (count === 0) {
    return { titleBarStyle: 'default', isolated: false, workbenchAccepted: false };
  }
  if (
    count !== 1 ||
    options.some((value) => /^--?(?:user-data-dir|extensions-dir)(?:=|$)/.test(value))
  ) {
    throw new Error(
      '--native-title-bar requires exactly one flag and no existing --user-data-dir or --extensions-dir'
    );
  }

  // mkdir without recursive/exist_ok is the ownership boundary: even a collision
  // cannot reuse someone else's directory or overwrite their settings. Keep this
  // diagnostic profile for inspection; callers may remove the reported path later.
  // Code-OSS also places Unix-domain sockets below this path. macOS's long
  // per-user tmpdir would exhaust sockaddr_un before the actual startup probe.
  const directory = path.join(
    process.platform === 'darwin' ? '/tmp' : tmpdir(),
    `ef-code-${randomUUID()}`
  );
  mkdirSync(directory, { mode: 0o700 });
  const user = path.join(directory, 'User');
  mkdirSync(user, { mode: 0o700 });
  const extensions = path.join(directory, 'extensions');
  mkdirSync(extensions, { mode: 0o700 });
  writeFileSync(
    path.join(user, 'settings.json'),
    `${JSON.stringify(
      {
        'window.titleBarStyle': 'native',
        ...(process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP === '1' &&
        typeof process.versions.mite === 'string'
          ? { 'keyboard.touchbar.enabled': false }
          : {})
      },
      null,
      2
    )}\n`,
    { flag: 'wx', mode: 0o600 }
  );
  argv.splice(argv.indexOf('--native-title-bar', 3), 1);
  // Insert before a caller's terminator: everything after it remains a file
  // argument, including strings which happen to spell probe-owned flags.
  const boundary = argv.indexOf('--', 3);
  argv.splice(
    boundary === -1 ? argv.length : boundary,
    0,
    '--user-data-dir',
    directory,
    '--extensions-dir',
    extensions
  );
  return {
    titleBarStyle: 'native',
    isolated: true,
    userDataDirectory: directory,
    extensionsDirectory: extensions,
    retainedForInspection: true,
    workbenchAccepted: false
  };
}
