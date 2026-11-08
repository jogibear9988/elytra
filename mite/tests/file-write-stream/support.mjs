/** Shared observation helpers; every positive workflow writes through the real OS filesystem. */
export function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

export function observe(stream) {
  const events = [];
  for (const name of ['open', 'ready', 'drain', 'finish', 'close']) {
    stream.on(name, () => events.push(name));
  }

  stream.on('error', (error) => events.push('error:' + error.code));

  return events;
}

export function event(stream, name) {
  return new Promise((resolve) => stream.once(name, resolve));
}

export function rejected(action) {
  try {
    action();
  } catch (error) {
    return error.code;
  }
  throw Error('Expected explicit error');
}
