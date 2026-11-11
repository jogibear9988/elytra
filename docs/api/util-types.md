# Native type predicates

`node:util/types` and `util.types` share one object and native functions.
Implemented predicates are isArrayBuffer, isSharedArrayBuffer, isAnyArrayBuffer,
isArrayBufferView, isTypedArray, isUint8Array, isDataView, isProxy and isPromise.
They inspect genuine LibJS class/storage slots without reading constructors,
prototypes, toStringTag or author getters. Subclasses retain their engine kind;
proxies do not acquire the target's brand, and revoked proxies remain proxies.

The focused actual Node/Mite fixture passes primitives, buffers, typed views,
shared storage, Promise and live/revoked/spoofing proxies on the current macOS
build. Other predicates and fresh Linux/Windows acceptance remain unfinished.
