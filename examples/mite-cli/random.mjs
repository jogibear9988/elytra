import { randomBytes, randomUUID, randomInt } from 'node:crypto';

const bytes = await new Promise((resolve, reject) => {
  randomBytes(24, (error, value) => (error ? reject(error) : resolve(value)));
});
console.log(
  JSON.stringify(
    { uuid: randomUUID(), token: bytes.toString('hex'), die: randomInt(1, 7) },
    null,
    2
  )
);
