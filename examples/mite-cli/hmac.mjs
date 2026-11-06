import { createHmac, timingSafeEqual } from 'node:crypto';
import { Buffer } from 'node:buffer';
import process from 'node:process';

const key = process.env.MITE_HMAC_KEY;
const [message = 'Hello from Mite', supplied] = process.argv.slice(2);
if (!key) {
  throw new Error('Set MITE_HMAC_KEY before running this example');
}

const signature = createHmac('sha256', key).update(message, 'utf8').digest();
if (supplied === undefined) {
  console.log(signature.toString('hex'));
} else {
  // Validate public format/length before comparing the fixed-size MAC bytes.
  const valid =
    supplied.length === 64 &&
    /^[0-9a-f]{64}$/i.test(supplied) &&
    timingSafeEqual(signature, Buffer.from(supplied, 'hex'));
  console.log(valid ? 'VALID' : 'INVALID');
  if (!valid) {
    process.exitCode = 1;
  }
}
