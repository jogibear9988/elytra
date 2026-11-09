/** Only genuine Web APIs; each child executes in its own document and origin. */
const query = new URL(location.href).searchParams;
const phase = query.get('phase');

const report = (value) =>
  fetch('/report', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(value)
  });

try {
  if (phase === 'query') {
    const permission = await navigator.permissions.query({
      name: query.get('permission') ?? 'geolocation'
    });
    await report({
      phase,
      tag: query.get('tag') ?? 'top',
      state: permission.state,
      url: location.href
    });
    if (!query.has('tag')) {
      for (const [tag, origin] of [
        ['same', location.origin],
        ['cross', query.get('cross')]
      ]) {
        const frame = document.createElement('iframe');
        frame.allow = 'geolocation ' + origin;
        frame.src = origin + '/page.html?phase=query&tag=' + tag;
        document.body.append(frame);
      }
    }
  } else if (phase === 'idle') {
    await report({ phase, tag: query.get('tag'), url: location.href });
  } else if (phase === 'request' || phase === 'hold') {
    navigator.geolocation.getCurrentPosition(
      () => report({ phase, error: 'Unexpected location grant' }),
      (failure) => report({ phase, tag: query.get('tag'), code: failure.code, url: location.href }),
      { timeout: phase === 'hold' ? 15000 : 3000, maximumAge: 0 }
    );
  } else {
    throw Error('Unknown fixture phase');
  }
} catch (failure) {
  await report({ phase, error: String(failure) });
}
