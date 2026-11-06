/** Install Fetch globals on the native runtime, independently of LibWeb and Node. */
import createHeaders from './headers.mjs';
import createBodies from './body.mjs';
import createObjects from './objects.mjs';
import createTransport from './transport.mjs';

export default function createFetchGlobals(host) {
  host.unsupported = (name) =>
    Object.assign(new Error(`${name} is not implemented`), { code: 'ERR_MITE_UNSUPPORTED_API' });

  const headers = createHeaders();
  const bodies = createBodies(host);
  const objects = createObjects(host, headers, bodies);
  return {
    Headers: headers.Headers,
    Request: objects.Request,
    Response: objects.Response,
    fetch: createTransport(host, headers, bodies, objects)
  };
}
