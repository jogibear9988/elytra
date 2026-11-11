/** Shared native brands: separate builtin and util.types receive the same owner-created object. */
export default function createUtilTypes(host) {
  const predicates = [
    'isArrayBuffer',
    'isSharedArrayBuffer',
    'isAnyArrayBuffer',
    'isArrayBufferView',
    'isTypedArray',
    'isUint8Array',
    'isDataView',
    'isProxy',
    'isPromise'
  ];
  if (!host.native || predicates.some((name) => typeof host.native[name] !== 'function')) {
    const error = new Error('Native util/types predicates are unavailable');
    error.code = 'ERR_MITE_UNSUPPORTED_API';
    throw error;
  }
  return host.native;
}
