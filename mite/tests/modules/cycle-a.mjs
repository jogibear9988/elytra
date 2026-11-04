import { readB } from './cycle-b.mjs';
export const a = 'A';
export function readA() { return a; }
console.log(readB());
