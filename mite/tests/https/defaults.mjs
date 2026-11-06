/** Public constructors retain Node identities/default ports and cannot accept private plans. */
import https from 'node:https';
import http from 'node:http';
let rejected = 0;
for (const invoke of [
  () => https.request('http://localhost/'),
  () => http.request('https://localhost/'),
  () => https.request({ protocol: 'http:' }),
  () => https.request({ protocol: 'ftp:' })
]) {
  try {
    invoke();
    throw Error('Protocol crossed transport boundary');
  } catch (error) {
    if (error.code !== 'ERR_INVALID_PROTOCOL') {
      throw error;
    }
    rejected++;
  }
}
const secure = https.request({ host: 'localhost', agent: false });
const ordinary = new http.ClientRequest(
  { host: 'localhost', agent: false },
  undefined,
  undefined,
  {},
  {
    protocol: 'https:',

    createSocket() {
      throw Error('Untrusted transport plan accepted');
    }
  }
);

secure.on('error', () => {});

ordinary.on('error', () => {});

console.log(
  'defaults',
  rejected,
  secure instanceof http.ClientRequest &&
    secure.protocol === 'https:' &&
    secure.getHeader('host') === 'localhost',
  ordinary.protocol === 'http:' && ordinary.getHeader('host') === 'localhost'
);
secure.destroy();
ordinary.destroy();
