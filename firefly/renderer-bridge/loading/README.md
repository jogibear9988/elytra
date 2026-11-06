# Application file modules

FileModules permits CORS-mode script requests from a local file Window client to
reach Ladybird's existing file subresource loader. It remains disabled in ordinary
browser and diagnostic helpers; the production service enables it at startup and
clears it at shutdown. No script/native API can change the switch.

The adapter preserves existing MIME checks, CSP, native file loading and sandbox
boundaries. It does not allow programmatic file fetch, network origins, opaque
clients, worker clients or non-script destinations. File script loading is not an
application-root filesystem sandbox; protect renderer documents as application
content and authorize privileged IPC independently.

Targets.cmake composes exactly one checked Fetching translation unit. The Python
generator verifies the pinned upstream fingerprint, rejects missing/duplicate
conditions and proves byte preservation outside the owned include/condition.
Keep Ladybird unchanged and review this seam when updating the pin.
