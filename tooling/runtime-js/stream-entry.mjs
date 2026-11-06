/** Configure the portable stream implementation for this native host. */
import stream from 'readable-stream/lib/stream.js';
import installWebAdapters from '../../mite/runtime/bootstrap/streams/web-adapters.mjs';

export default function createStreamModule(host) {
  stream.setDefaultHighWaterMark(false, host.platform === 'win32' ? 16384 : 65536);
  installWebAdapters(stream, host);
  return stream;
}
