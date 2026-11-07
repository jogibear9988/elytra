# Native navigation resource session

The owned helper installs a native control receiver after VM initialization.
It creates no fetch provider until the trusted UI sends a valid endpoint and
navigation binding. There is no page, preload, environment or BiDi setter.

The receiving ConnectionFromClient's own PageHost must contain the named page;
that page must contain the named local top-level navigable with the current native
navigation ID. The URI must satisfy the native Navigation scope. Epochs increase
per helper, duplicates cannot replace an admitted navigation, and at most 32 live
navigables have records. Destroyed records are swept. Completed clients are revoked
and release VM/transport state while the last ID stays attached to the live native
navigable to prevent replay. An unused admission expires after five seconds.

The provider has only a navigation handler. Ordinary document fetches receive no
resource capability from it. The native one-shot client applies the remaining
request/client/URL/body checks; the parent FileServer authorizes each file path.

Both production and diagnostic owned helpers use the session. The unmodified
helper installs no receiver. Shutdown removes the receiver/provider and revokes
all clients before releasing the records. On an upstream update, inspect the real
ConnectionFromClient -> WebContentServerStub inheritance used by the private
NotRouted receiver, its PageHost ownership and the UI message ordering.

See [ADR 0122](../../../docs/architecture/0122-live-helper-resource-navigation.md)
for actual helper/DOM evidence and the still-missing post-commit resource binding.
