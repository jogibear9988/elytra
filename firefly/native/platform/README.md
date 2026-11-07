# Desktop service boundary

DesktopServices.h binds available native services into the trusted Mite host.
Bridge.cpp delegates service installation and change notifications here; it
continues to own windows, preload/IPC admission, callback queues and teardown.

Mac.cpp uses the existing AppKit/CoreGraphics screen/theme bindings and connects
their watches to the bridge's QObject owner. Clipboard and geometry provider
selection stays in the native runtime CMake source list.

Linux.cpp requires explicit X11 startup and refuses unfinished display/theme
operations with ERR_FIREFLY_UNSUPPORTED_API. It exposes no fabricated snapshots
or polling-based notifications. The experimental runtime links the actual X11
clipboard and Linux geometry adapter. Native end-to-end Linux acceptance is
pending; Windows desktop is still unsupported.

The environment probe calls only the real startup check, before constructing
any GUI application. The Linux service library is also compiled on macOS for
header/link validation, without replacing the macOS provider. Host bindings and
their capability fields are private main-process implementation details; the
renderer still receives only the separately validated preload/IPC surface.

RuntimePaths.cpp isolates filesystem discovery from the VM/GUI. macOS selects
its existing bundle layout; Linux accepts only the dedicated firefly-runtime
helper/resource tree relative to the actual executable, then uses the configured
build directory as a development fallback. A sibling upstream bin/WebContent
cannot satisfy the Linux selector. The standalone native path probe exercises
relocation and incomplete/decoy layouts without any GUI initialization. The
Linux app gate additionally requires the observed live renderer to execute from
the relocated tree; its hosted acceptance and distributable dependency closure
remain pending.
