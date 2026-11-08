/** Invalid arguments must fail before a shell can create the sentinel file. */
import { exec } from 'node:child_process';
import fs from 'node:fs';
import { check, python } from './support.mjs';

const command = python("open('validation-executed.bin','w').write('unexpected execution')");
const cases = [
  ['missing-command', () => exec()],
  ['empty-command', () => exec('')],
  ['null-command', () => exec(null)],
  ['number-command', () => exec(42)],
  ['boxed-command', () => exec(new String(command))],
  ['nul-command', () => exec('echo\0never')],
  ['callback', () => exec(command, {}, 42)],
  ['cwd', () => exec(command, { cwd: {} })],
  ['negative-timeout', () => exec(command, { timeout: -1 })],
  ['fractional-timeout', () => exec(command, { timeout: 1.5 })],
  ['string-timeout', () => exec(command, { timeout: '1' })],
  ['nan-timeout', () => exec(command, { timeout: NaN })],
  ['negative-budget', () => exec(command, { maxBuffer: -1 })],
  ['string-budget', () => exec(command, { maxBuffer: '1' })],
  ['nan-budget', () => exec(command, { maxBuffer: NaN })],
  ['kill-signal', () => exec(command, { killSignal: 'BAD' })],
  ['nul-shell', () => exec(command, { shell: 'invalid\0shell' })]
];
const observations = [];
for (const [name, action] of cases) {
  let failure;
  try {
    const child = action();

    child.on('error', () => {});
  } catch (error) {
    failure = error;
  }
  check(failure instanceof Error, 'Validation before shell execution: ' + name);
  observations.push([name, failure.name, failure.code]);
}
await new Promise((resolve) => setTimeout(resolve, 30));
check(!fs.existsSync('validation-executed.bin'), 'No invalid call executes a shell');
console.log(JSON.stringify(observations));
