/** Hosted-only actual clipboard acceptance; modifies the isolated CI desktop clipboard. */
const { app, clipboard, ClipboardItem } = await import(process.env.FIREFLY_API_MODULE);

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

try {
  check(
    process.env.FIREFLY_ALLOW_CLIPBOARD_TEST === '1',
    'clipboard fixture requires explicit CI opt-in'
  );
  await app.whenReady();
  await clipboard.writeText('Elytra Grüße 🪲');
  check((await clipboard.readText()) === 'Elytra Grüße 🪲', 'native unicode roundtrip');
  check(await clipboard.has('text/plain'), 'native MIME metadata');
  await clipboard.write([
    new ClipboardItem({
      'text/plain': Promise.resolve('clipboard entry'),
      'text/html': new Blob(['<b>clipboard entry</b>'], { type: 'text/html' }),
      'text/rtf': '{\\rtf1\\ansi clipboard entry}'
    })
  ]);
  const [item] = await clipboard.read();
  for (const mime of ['text/plain', 'text/html', 'text/rtf']) {
    check(item.types.includes(mime), 'native representation ' + mime);
    check(
      (await (await item.getType(mime)).text()).includes('clipboard entry'),
      'native payload ' + mime
    );
  }
  clipboard.clear();
  check((await clipboard.readText()) === '', 'native clear');
  console.log(JSON.stringify({ clipboard: true, unicode: true, representations: 3 }));
  app.quit();
} catch (error) {
  console.error(error.stack);
  app.exit(1);
}
