/** Read plain text, or explicitly copy one text/HTML entry with --copy. */
import { app, clipboard, ClipboardItem } from 'firefly';

await app.whenReady();
if (process.argv.includes('--copy')) {
  await clipboard.write([
    new ClipboardItem({
      'text/plain': 'Hello from Firefly',
      'text/html': '<b>Hello from Firefly</b>'
    })
  ]);
  console.log('Copied a text and HTML entry');
} else {
  console.log(await clipboard.readText());
}
app.quit();
