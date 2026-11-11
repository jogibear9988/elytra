/**
 * Limited inspector compatibility: query the actual inactive native endpoint.
 * Debugger activation, attachment and sessions require an unimplemented backend.
 * This module deliberately does not emulate V8 or Chrome DevTools Protocol.
 */
export default function createInspectorModule(host) {
  const query = host.native.url;

  function unavailable(operation) {
    throw Object.assign(
      new Error(`inspector.${operation} requires an unavailable debugger backend`),
      {
        code: 'ERR_MITE_UNSUPPORTED_API'
      }
    );
  }

  function url() {
    return query();
  }

  function open(port, address, wait) {
    return unavailable('open');
  }

  function close() {
    return unavailable('close');
  }

  function waitForDebugger() {
    return unavailable('waitForDebugger');
  }

  class Session {
    constructor() {
      unavailable('Session');
    }
  }
  return { url, open, close, waitForDebugger, Session };
}
