const a = await import('./identity.mjs?x#one');
const b = await import('./identity.mjs?x#one');
const c = await import('./identity.mjs?x#two');
const d = await import('./identity.mjs?y#one');
console.log(a === b, a === c, a === d);
console.log(a.instance, c.instance, d.instance);
