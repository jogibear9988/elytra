/** Real fork peer. Every mode is bounded and owns no independent grandchildren. */
import { writeFileSync, existsSync } from 'node:fs';
import { createHash } from 'node:crypto';

const [mode, ...args] = process.argv.slice(2);
const watchdog = setTimeout(() => {
  throw Error('Fork child timed out: ' + mode);
}, 8000);
watchdog.unref();
if (mode === 'idle') {
  process.send({ ready: mode });
} else if (mode === 'unref-channel') {
  process.on('message', () => {
    throw Error('Unexpected message');
  });

  process.channel.unref();
  process.send({ ready: mode });
} else if (mode === 'independent') {
  // Survive parent teardown long enough to write the test-owned acknowledgement.
  const deadline = Date.now() + 5000;
  const polling = setInterval(() => {
    if (existsSync(args[1])) {
      writeFileSync(args[0], 'CHILD_SURVIVED');
      clearInterval(polling);
    } else if (Date.now() >= deadline) {
      clearInterval(polling);
      process.exitCode = 1;
    }
  }, 20);
} else if (mode === 'delayed') {
  setTimeout(() => {
    process.on('message', (value) => {
      process.send({ value }, () => process.disconnect());
    });
  }, 50);
} else if (mode === 'sink') {
  let count = 0;
  const hash = createHash('sha256');

  process.on('message', (value) => {
    count++;
    hash.update(JSON.stringify(value));
  });

  process.on('disconnect', () => {
    console.log(JSON.stringify({ count, hash: hash.digest('hex') }));
  });
} else if (mode === 'stdio') {
  console.log('FORK_STDOUT');
  console.error('FORK_STDERR');
  process.send({
    ready: mode,
    args,
    cwd: process.cwd(),
    token: process.env.FORK_TOKEN,
    connected: process.connected,
    send: typeof process.send,
    channel:
      typeof process.channel?.ref === 'function' && typeof process.channel?.unref === 'function'
  });
} else if (mode === 'hold') {
  const listener = (value) => {
    if (value !== 'release') {
      throw Error('Unexpected release');
    }
    process.removeListener('message', listener);
  };

  process.on('message', listener);
  process.send({ ready: mode });
} else {
  let seq = 0;

  process.on('message', (value) => {
    if (value?.command === 'finish') {
      process.send({ done: true, count: seq }, () => process.disconnect());
    } else {
      process.send({ seq: seq++, value });
    }
  });

  process.send({ ready: 'echo' });
}
