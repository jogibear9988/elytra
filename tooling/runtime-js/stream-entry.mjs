/** Configure the portable stream implementation for this native host. */
import stream from 'readable-stream/lib/stream.js';

export default function createStreamModule(host) {
  stream.setDefaultHighWaterMark(false, host.platform === 'win32' ? 16384 : 65536);
  return stream;
}
