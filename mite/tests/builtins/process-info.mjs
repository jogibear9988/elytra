import P, { cwd, env, pid, ppid, platform, arch } from 'node:process';
import Alias from 'process';
import CJS from './process-export.cjs';
console.log(P === globalThis.process, P === Alias, P === CJS, cwd === P.cwd, env === P.env);
console.log(
  pid === P.pid,
  ppid === P.ppid,
  Number.isInteger(pid) && pid > 0,
  Number.isInteger(ppid) && ppid > 0
);
console.log(['win32', 'darwin', 'linux'].includes(platform), ['arm64', 'x64'].includes(arch));
console.log(P.argv[0] === P.execPath, typeof P.argv0 === 'string' && P.argv0.length > 0);
console.log(typeof P.versions.uv === 'string', P.version.startsWith('v'));
