/** Real shell execution, capture, termination and promisification; no mocked child processes. */
import childProcess, { exec, ChildProcess } from 'node:child_process';
import { promisify } from 'node:util';
import { check, python, quote } from './support.mjs';

const namespace = await import('node:child_process');
check(namespace.exec === exec && childProcess.exec === exec, 'Exec export identity');
const rows = {};

async function execute(command, options = {}, input, allowSpawnFailure = false) {
  let count = 0;
  let immediate = true;
  let result;
  const events = [];
  let child;
  const closed = new Promise((resolve, reject) => {
    child = exec(command, options, (error, stdout, stderr) => {
      check(!immediate, 'Exec callback must be asynchronous');
      ++count;
      events.push('callback');
      if (error) {
        check(error.cmd === command, 'Error cmd preserves the original shell command');
      }
      result = {
        stdout: Buffer.isBuffer(stdout) ? { buffer: true, hex: stdout.toString('hex') } : stdout,
        stderr: Buffer.isBuffer(stderr) ? { buffer: true, hex: stderr.toString('hex') } : stderr,
        error: error
          ? {
              name: error.name,
              code: error.code,
              killed: error.killed,
              signal: error.signal,
              cmd: error.cmd,
              errno: error.errno,
              path: error.path,
              syscall: error.syscall,
              spawnargs: error.spawnargs
            }
          : null
      };
    });
    check(
      child instanceof ChildProcess && child.stdin && child.stdout && child.stderr,
      'Actual ChildProcess and native pipes'
    );

    child.on('error', (error) => {
      events.push('error');
      if (!allowSpawnFailure) {
        reject(error);
      }
    });

    child.once('close', () => {
      events.push('close');
      if (count !== 1) {
        reject(Error('Exec callback count: ' + count));
      } else {
        resolve({ ...result, events });
      }
    });

    child.stdin.on('error', (error) => {
      if (error.code !== 'EPIPE') {
        reject(error);
      }
    });

    child.stdin.end(input);
  });
  immediate = false;
  return closed;
}

rows.whitespace = await execute('   ');
rows.quoted = await execute(
  python("import sys;print(sys.argv[1],end='')") +
    ' ' +
    quote(process.platform === 'win32' ? 'spaces ascii value' : 'spaces ä "double" \'single\'')
);
rows.pipeline = await execute(
  process.platform === 'win32'
    ? 'echo shell-pipe|findstr shell-pipe'
    : "printf 'shell-pipe' | tr a-z A-Z"
);
rows.environment = await execute(
  process.platform === 'win32' ? 'echo %EXEC_TOKEN%' : 'printf %s "$EXEC_TOKEN"',
  { env: { ...process.env, EXEC_TOKEN: 'owned value ä' }, cwd: process.cwd() }
);
rows.cwd = await execute(python("import os;print(os.getcwd(),end='')"), { cwd: process.cwd() });
rows.customShell = await execute(python("print('custom shell',end='')"), {
  shell: process.platform === 'win32' ? process.env.ComSpec || 'cmd.exe' : '/bin/sh'
});
if (process.platform !== 'win32') {
  rows.emptyShellDirect = await execute('pwd', { shell: '' });
  rows.emptyShellLiteralFailure = await execute('printf a', { shell: '' }, undefined, true);
  check(
    rows.emptyShellLiteralFailure.error?.code === 'ENOENT',
    'Empty shell treats a spaced command as a literal executable path'
  );
}
rows.binary = await execute(
  python(
    'import sys;sys.stdout.buffer.write(bytes([0,128,255]));sys.stderr.buffer.write(bytes([255,0]))'
  ),
  { encoding: 'buffer' }
);
rows.stdin = await execute(
  python('import sys;sys.stdout.buffer.write(sys.stdin.buffer.read())', true),
  {},
  'through native stdin ä'
);
rows.nonzero = await execute(
  python("import sys;sys.stdout.write('OUT');sys.stderr.write('ERR');sys.exit(7)")
);
rows.limit = await execute(
  python("import sys;sys.stdout.write('abcdef');sys.stdout.flush()", true),
  { maxBuffer: 3 }
);
rows.stderrLimit = await execute(
  python("import sys;sys.stderr.write('abcdef');sys.stderr.flush()", true),
  { maxBuffer: 3 }
);
// POSIX exec replaces the shell, so the bounded timeout cannot strand an orphaned Python child.
rows.timeout = await execute(python('import time;time.sleep(0.4)', true), { timeout: 30 });
check(rows.timeout.error?.killed === true, 'Timeout terminates the actual child');
rows.overload = await new Promise((resolve, reject) => {
  exec(python("print('callback overload',end='')"), (error, stdout, stderr) => {
    if (error) {
      reject(error);
    } else {
      resolve({ stdout, stderr });
    }
  });
});
rows.noCallback = await new Promise((resolve, reject) => {
  const child = exec(python('import sys;sys.exit(3)'));
  child.on('error', reject);

  child.once('close', (code, signal) => resolve([code, signal]));
});
const promisedExec = promisify(exec);
const promise = promisedExec(
  python("import sys;sys.stdout.write('promise');sys.stderr.write('side')")
);
check(promise.child instanceof ChildProcess, 'Promisified exec immediately exposes the real child');
rows.promisified = await promise;
try {
  await promisedExec(
    python("import sys;sys.stdout.write('failure-out');sys.stderr.write('failure-err');sys.exit(9)")
  );
  throw Error('Expected promisified shell failure');
} catch (error) {
  check(
    error.code === 9 && error.stdout === 'failure-out' && error.stderr === 'failure-err',
    'Promisified failure attaches captured output'
  );
  rows.promisifiedFailure = { code: error.code, stdout: error.stdout, stderr: error.stderr };
}
check(
  rows.nonzero.error.code === 7 && rows.nonzero.stdout === 'OUT' && rows.nonzero.stderr === 'ERR',
  'Nonzero shell captures both streams'
);
check(
  rows.limit.error.code === 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER' && rows.limit.stdout === 'abc',
  'Stdout byte budget'
);
check(
  rows.stderrLimit.error.code === 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER' &&
    rows.stderrLimit.stderr === 'abc',
  'Stderr byte budget'
);
console.log(JSON.stringify(rows));
