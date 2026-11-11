# Diagnostics channels and debug logging

`node:diagnostics_channel` provides real channel subscriptions, publication,
unsubscription and subscriber queries. Named channels use weak inactive lifetimes
and retain active subscriptions/stores. Channel names, aliases and callback order
follow the maintained Node24 source pinned with its license under
vendor/node-diagnostics-channel.

Tracing channels publish start/end/error and asynchronous lifecycle events around
real sync, Promise and callback work. Bound stores use actual AsyncLocalStorage.run;
this is in-process tracing, not a fabricated native instrumentation stream.
Subscriber/transform failures reach process error handling through nextTick.

`util.debuglog` and `util.debug` share NODE_DEBUG wildcard/case matching, cached
categories, enabled queries and lazy initialization callbacks. Enabled loggers write
real stderr with category/process prefixes; disabled categories do not write.
Formatting uses the bounded Console implementation, not V8 inspection or colors.

Focused Node/reference and actual Mite fixtures pass subscription changes,
tracing/error order, bound async stores, enabled/disabled matching and stderr output.
Full external instrumentation and fresh Linux/Windows acceptance remain unverified.
