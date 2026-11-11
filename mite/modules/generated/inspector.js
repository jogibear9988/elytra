/* Generated from mite/modules/system/inspector.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/system/inspector.mjs
function createInspectorModule(host) {
  const query = host.native.url;
  function unavailable(operation) {
    throw Object.assign(
      new Error(`inspector.${operation} requires an unavailable debugger backend`),
      {
        code: "ERR_MITE_UNSUPPORTED_API"
      }
    );
  }
  function url() {
    return query();
  }
  function open(port, address, wait) {
    return unavailable("open");
  }
  function close() {
    return unavailable("close");
  }
  function waitForDebugger() {
    return unavailable("waitForDebugger");
  }
  class Session {
    constructor() {
      unavailable("Session");
    }
  }
  return { url, open, close, waitForDebugger, Session };
}
return createInspectorModule;
})()
