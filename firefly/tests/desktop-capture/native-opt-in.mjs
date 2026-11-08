/** Opt-in native metadata acceptance. This may request OS screen-recording permission.
 * Never include this entry in unattended desktop tests; it captures no image pixels.
 */
import { app, desktopCapturer, screen } from 'firefly';

await app.whenReady();
let status = 1;
try {
  const displays = new Set(screen.getAllDisplays().map((display) => String(display.id)));
  const sources = await desktopCapturer.getSources({
    types: ['screen'],
    thumbnailSize: { width: 0, height: 0 }
  });
  if (
    sources.length === 0 ||
    !sources.every(
      (source) =>
        displays.has(source.display_id) &&
        source.id === `screen:${source.display_id}:0` &&
        source.thumbnail.isEmpty() &&
        source.thumbnail.toPNG().length === 0 &&
        source.appIcon === null
    )
  ) {
    throw Error('Native screen sources do not match active native display identities');
  }
  console.log(
    'PASS opt-in actual OS source enumeration and native display identity; no pixel/media capture'
  );
  status = 0;
} catch (error) {
  console.error(error.code ?? error.name, error.message);
} finally {
  app.exit(status);
}
