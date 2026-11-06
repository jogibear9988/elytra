/**
 * fetch-blob 3.2.0's optional Node stream fallback is replaced at bundle resolution.
 * The runtime already supplies captured LibJS Web Streams/codecs; no Node shim or
 * second stream polyfill is needed. The package's immutable byte core stays intact.
 */
export {};
