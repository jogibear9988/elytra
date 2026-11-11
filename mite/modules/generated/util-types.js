/* Generated from mite/modules/core/util-types.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/util-types.mjs
function createUtilTypes(host) {
  const predicates = [
    "isArrayBuffer",
    "isSharedArrayBuffer",
    "isAnyArrayBuffer",
    "isArrayBufferView",
    "isTypedArray",
    "isUint8Array",
    "isDataView",
    "isProxy",
    "isPromise"
  ];
  if (!host.native || predicates.some((name) => typeof host.native[name] !== "function")) {
    const error = new Error("Native util/types predicates are unavailable");
    error.code = "ERR_MITE_UNSUPPORTED_API";
    throw error;
  }
  return host.native;
}
return createUtilTypes;
})()
