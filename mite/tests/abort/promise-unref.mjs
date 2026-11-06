/** Unreferenced Promise timers do not turn pending JS promises into native liveness. */
import { setTimeout } from 'node:timers/promises';
setTimeout(5000, 'UNEXPECTED', { ref: false }).then(console.log);
console.log('exit');
