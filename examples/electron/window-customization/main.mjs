/**
 * Window customization main-process reference. Frameless/drag regions use the actual native
 * adapter. This smoke checks construction/page startup; drag gestures and HTML native min/max
 * controls have separate existing Designer/native tests. Title-bar
 * overlay/transparency/fullscreen are not implied.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'window-customization',
  requires: [],
  earlyRequires: [],
  smoke: true,
  options: { frame: false }
});
