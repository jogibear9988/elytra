/** A trace uses the genuine captured call stack and the actual stderr stream. */
import { deprecate } from 'node:util';
import process from 'node:process';
process.traceDeprecation = true;

function ownedWarningCaller() {
  deprecate(() => {}, 'owned trace warning', 'OWNED_TRACE')();
}

ownedWarningCaller();
console.log('PASS traceDeprecation warning scheduled');
