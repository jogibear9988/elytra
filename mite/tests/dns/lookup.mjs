import dns, { lookup } from 'node:dns';
import p, { lookup as promiseLookup } from 'node:dns/promises';
console.log('identity', dns.lookup === lookup, dns.promises === p, p.lookup === promiseLookup);
for (const hostname of ['127.0.0.1', '::1', 'localhost']) {
  for (const options of [
    { family: 4 },
    { family: 6 },
    { all: true, order: 'ipv4first' },
    { all: true, order: 'ipv6first' },
    { all: true, order: 'verbatim' }
  ]) {
    const value = await p.lookup(hostname, options);
    console.log(hostname, JSON.stringify(options), JSON.stringify(value));
  }
}
for (const hostname of ['127。0。0。1', '１２７.０.０.１']) {
  console.log('idna', JSON.stringify(await p.lookup(hostname, { family: 4 })));
}
let sync = true;

lookup('127.0.0.1', (err, address, family) =>
  console.log('async numeric', sync, err, address, family)
);

sync = false;
await new Promise((resolve) =>
  lookup('localhost', 4, (err, address, family) => {
    console.log('callback', err, address, family);
    resolve();
  })
);
for (const value of ['verbatim', 'ipv4first', 'ipv6first']) {
  dns.setDefaultResultOrder(value);
  console.log('order', dns.getDefaultResultOrder(), p.getDefaultResultOrder());
  console.log('default', JSON.stringify(await p.lookup('localhost', { all: true })));
}
p.setDefaultResultOrder('verbatim');
console.log(dns.getDefaultResultOrder());
for (const hostname of ['bad host', 'invalid#hostname']) {
  try {
    await p.lookup(hostname);
  } catch (e) {
    console.log('error', e.name, e.code, e.errno, e.syscall, e.hostname === hostname);
  }
}
