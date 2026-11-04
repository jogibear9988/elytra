import { metadata } from './metadata.mjs';
console.log(import.meta.url.endsWith('/meta.mjs'));
console.log(import.meta.filename.endsWith('meta.mjs'));
console.log(import.meta.dirname.endsWith('modules'));
console.log(import.meta.main, metadata.main);
console.log(Object.getPrototypeOf(import.meta) === null);
