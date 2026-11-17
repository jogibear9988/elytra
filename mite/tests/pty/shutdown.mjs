/** Owner-session teardown must reap its directly owned PTY child. */
import { spawn } from 'mite:pty';
import { fileURLToPath } from 'node:url';
const [, python, cwd, nonce, record] = process.argv.slice(2);
const terminal = spawn(
  python,
  ['-u', fileURLToPath(new URL('./peer.py', import.meta.url)), nonce + '-shutdown', record],
  {
    name: 'xterm-256color',
    cwd,
    env: { ...process.env, ELYTRA_OWNED_PTY: nonce + '-shutdown' },
    cols: 80,
    rows: 24
  }
);
let raw = '';
const deadline = setTimeout(() => {
  throw Error('PTY shutdown fixture deadline');
}, 8000);

terminal.onData((data) => {
  raw += data;
  if (raw.includes('@@PTY ')) {
    clearTimeout(deadline);
    console.log('PASS native session shutdown with a live directly owned PTY child');
    process.exit(0);
  }
});
