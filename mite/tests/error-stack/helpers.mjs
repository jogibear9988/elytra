/** Assertions normalize only engine formatting, never the identity/order of actual frames. */
import assert from 'node:assert/strict';

export function frames(stack) {
  assert.equal(typeof stack, 'string');
  return stack
    .split('\n')
    .slice(1)
    .filter((line) => line.trim());
}

export function assertOrder(stack, names) {
  const lines = frames(stack);
  let previous = -1;
  for (const name of names) {
    const index = lines.findIndex((line) => new RegExp(`\\b${name}\\b`).test(line));
    assert.ok(index > previous, `Missing or misordered actual frame ${name}: ${stack}`);
    previous = index;
  }
  return lines;
}

export function stackCaptureLeaf(target, constructorOpt) {
  assert.equal(Error.captureStackTrace(target, constructorOpt), undefined);
  return target;
}

export function stackCaptureMiddle(target, constructorOpt) {
  return stackCaptureLeaf(target, constructorOpt);
}

export function stackCaptureTop(target, constructorOpt) {
  return stackCaptureMiddle(target, constructorOpt);
}
