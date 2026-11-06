/** Actual native pipe peer closure: end succeeds, but writing to the closed peer fails. */
import { spawn } from 'node:child_process';
import { once } from 'node:events';

const python = process.argv[2];
if (!python) {
  throw new Error('Pass the independent Python child executable');
}

for (const write of [false, false, false, false, true]) {
  // The acknowledgement is bytes, bypassing Python's Windows text newline conversion.
  const child = spawn(
    python,
    [
      '-c',
      "import os,sys,time;os.close(0);sys.stdout.buffer.write(b'CLOSED\\n');sys.stdout.buffer.flush();time.sleep(0.15)"
    ],
    { stdio: ['pipe', 'pipe', 'ignore'] }
  );
  const closed = once(child, 'close');
  const errors = [];

  child.stdin.on('error', (error) => errors.push(error.code));

  let output = '';
  for await (const chunk of child.stdout) {
    output += chunk;
    if (output === 'CLOSED\n') {
      if (write) {
        child.stdin.write('must fail');
      } else {
        child.stdin.end();
      }
    }
  }
  const [code, signal] = await closed;
  if (
    code !== 0 ||
    signal !== null ||
    output !== 'CLOSED\n' ||
    JSON.stringify(errors) !== JSON.stringify(write ? ['EPIPE'] : []) ||
    (!write && !child.stdin.writableFinished)
  ) {
    throw new Error(JSON.stringify({ write, code, signal, output, errors }));
  }
}
console.log('PASS native disconnected child stdin finish and preserved write EPIPE');
