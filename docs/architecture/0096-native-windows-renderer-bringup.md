# ADR 0096: Native Windows renderer bring-up before desktop API claims

Status: experimental native CI/build adapter implemented; compilation and execution
pending. Windows Firefly desktop/runtime/package support remains unfinished.

## Evidence at the current pin

Ladybird remains unmodified at 97ad425b5125f20c516e1a8d3b5bb02f391fc975.
Its build guide describes native clang-cl as experimental and does not support
MinGW/MSYS2. WSL is an upstream browser recommendation, not an acceptable runtime
or build fallback for this project. The actual pinned tree provides Windows
LibCore/process paths, helper targets and a Qt DirectX presentation source at
UI/Qt/WebContentViewWindows.cpp. Its manifest selects Windows Qt and dependencies.
That source presence proves a possible integration path, not native acceptance.

Mite already passes its standalone native clang-cl x64 CLI suites. Reusing that
fact cannot establish LibWeb, desktop windows or renderer sandbox behavior.

## Decision

Add an independent Windows Server 2025/Visual Studio clang-cl x64 workflow using
our existing pinned bootstrap, upstream CMake graph and owned diagnostic targets.
Keep it separate from Mite's concurrency group so long renderer dependencies do
not block standalone CLI acceptance. Preserve completed dependency packages after
failed/partial builds; do not restart a live build merely because observation is
slow. There is no alternate engine or Linux/WSL execution path.

First configure and compile the actual LibWebView renderer, service helpers and
owned isolation/wire probes. Apply upstream's ladybird_windows_bin(CONSOLE) to
our Windows diagnostic executables, preserving console output and native DLL
copying instead of relying on a developer PATH. This is an owned adapter call;
no upstream source/manifest is changed.

Require real headless LibJS DOM execution, absent ambient Node capabilities,
a different reported WebContent PID and a 640x400 compositor PNG through the
existing test_renderer.py. Require actual document/realm/GC callback isolation
and separate-process wire encode/decode checks. All failures fail the component
job; missing binaries are not skipped. Tests do not launch the Qt browser UI.

## Scope and next requirements

The renderer runner explicitly reports Windows process/sandbox checks UNVERIFIED.
The headless component gate must retain that distinction rather than advertise
complete Windows isolation or a production desktop release. Native executable/
parent/token/job/handle restrictions and independent live process observations
remain required, along with a native Qt window/input/compositor path, product
preload/IPC integration, desktop services, the Mite main process and packaging.
The production firefly/window/preload target guards remain unchanged.

Use real native failure diagnostics to select subsequent adapters. Do not weaken
existing renderer/isolation assertions or replace the required app with this probe.
Current Windows SDK support, other architectures/hosts, GPU/driver coverage,
Unicode packaged paths, actual Designer editing and release acceptance need their
own evidence. See the full [specification](../../mite-firefly-agent-instructions.md)
and [current support](../status.md).
