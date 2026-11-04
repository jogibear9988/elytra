const key = 'MITE_NATIVE_ENV_FIXTURE';
delete process.env[key];
console.log(process.env[key] === undefined, key in process.env);
process.env[key] = 42;
console.log(process.env[key], typeof process.env[key], key in process.env, Object.hasOwn(process.env, key));
process.env[key] = 'Grüße 🪲'.repeat(100);
console.log(process.env[key] === 'Grüße 🪲'.repeat(100), Object.keys(process.env).includes(key));
const descriptor = Object.getOwnPropertyDescriptor(process.env, key);
console.log(descriptor.writable, descriptor.enumerable, descriptor.configurable);
Object.defineProperty(process.env, key, { value: 'defined', writable: true, enumerable: true, configurable: true });
console.log(process.env[key]);
try { Object.defineProperty(process.env, key, { value: 'invalid' }); }
catch (error) { console.log(error.name, error.code); }
console.log(delete process.env[key], process.env[key] === undefined);
