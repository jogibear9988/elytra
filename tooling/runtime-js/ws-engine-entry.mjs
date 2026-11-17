/** Initialize the pinned private RFC 6455 client after captured native builtins are available.
 * The generated outer closure supplies require=host.requireBuiltin, Buffer=host.Buffer and
 * process=host.requireBuiltin('node:process'). Optional native addons are disabled by the
 * audited build's WS_NO_BUFFER_UTIL / WS_NO_UTF_8_VALIDATE constant definitions, never env edits.
 * The WHATWG facade owns public semantics and selects perMessageDeflate:false explicitly.
 */
export default function createWebSocketEngine(host) {
  if (typeof host.requireBuiltin !== 'function' || typeof host.Buffer !== 'function') {
    throw new TypeError('WebSocket engine requires captured builtin and Buffer providers');
  }
  // Lazy CJS initialization is intentional: top-level imports would evaluate the
  // provider before the factory receives its owner-realm dependency captures.
  return require('./node_modules/ws/lib/websocket.js');
}
