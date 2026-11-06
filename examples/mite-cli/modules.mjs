import settings from './greeting.json' with { type: 'json' };
console.log(`${settings.greeting}, ${process.argv[2] || 'world'}!`);
await new Promise((resolve) => setTimeout(resolve, 1));
console.log('JSON imports and top-level await work.');
