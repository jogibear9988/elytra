# Native Mite runtime

Mite owns a LibJS VM and a libuv event loop. The CLI and Firefly main process link
this same runtime; this directory must not depend on Firefly, LibWeb or Qt.
Application APIs are implemented by JavaScript compatibility modules backed by
small native hosts. The directories separate those layers by responsibility.

| Directory | Responsibility |
| --- | --- |
| core | VM/realm lifetime, event-loop dispatch, asynchronous ownership and embedding hooks |
| modules | ESM/CommonJS resolution, package rules, builtin caches and trusted host-module registration |
| bootstrap | Initial global installation and its embedded-source template |
| platform/process | Process identity/environment, subprocesses, pipes and termination |
| platform/filesystem | Filesystem operations, descriptors and native result/error conversion |
| platform/network | TCP, DNS and HTTP parsing/transport |
| platform/crypto | OS entropy and OpenSSL hash/HMAC/equality state |
| platform/system | Native OS information/constants and network-interface metadata |
| platform/text | URL and text-encoding adapters |
| adapters | Narrow, checked integration seams for unmodified upstream code |
| ../cli | Executable entry and native argument conversion |

Include runtime headers relative to this directory, for example
`#include "core/EventLoop.h"` or `#include "platform/network/DNS.h"`. Keep native
worker state separate from GC-managed values: JS values and callbacks belong to
the VM owner thread. Document who owns a handle, when it closes, and how pending
work is cancelled. Explain compatibility decisions rather than restating statements.

The explicit source list is in [Runtime.cmake](../cmake/Runtime.cmake). Add new
subsystems there, maintain the corresponding API/matrix and tests, and record the
step in [PROGRESS](../../docs/PROGRESS.md). Application-facing CommonJS support is
an intentional compatibility feature; it does not dictate how implementation
sources must be authored.
