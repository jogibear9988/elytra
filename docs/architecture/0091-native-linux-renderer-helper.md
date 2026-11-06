# ADR 0091: Native Linux renderer/helper acceptance before desktop expansion

Status: implemented experimental build path; native Linux hosted acceptance pending.

## Decision

Make the owned renderer helper available on Linux, retaining upstream's complete
native WebContent bootstrap and RendererSandboxLinux implementation. Select the
correct sandbox and bin/share-Lagom resource layout in an owned CMake module.
Keep macOS's existing bundle/signing path and reject unsupported helper platforms.
Do not enable Firefly Linux desktop APIs before its renderer/process foundation
and native window integration have working acceptance.

Generate a separate Linux renderer dependency manifest outside vendor. Add Qt's
native Linux features using upstream's own Qt/provider version overrides, instead
of depending on a distribution Qt below the current minimum or editing Ladybird.
Preserve original dependency bytes, pins, overlays and notices. Share the base
manifest reader with the existing Mite+browser graph, while keeping their native
I/O dependencies out of the standalone renderer manifest.

Reuse a composite Linux compiler/CMake/Rust setup action for runtime and renderer
jobs. The renderer workflow compiles actual ELF executables and requires existing
DOM/compositor/process, native isolation/wire and owned helper lifetime gates.
Observe actual kernel no-new-privileges/seccomp fields on the reported owned PID,
requiring a filter count above its parent's inherited filters. Never disable the
sandbox to produce a passing CI result. Activation is not a full containment audit.

## Validation boundaries and remaining work

Local CMake-script tests cover platform resource/sandbox selection, unsupported
Windows refusal and owned manifest pin/overlay/tamper guards. Existing macOS
helper/Firefly builds verify the shared refactor. These do not establish Linux
native execution. Hosted Linux compilation and real process/graphics/sandbox
tests remain mandatory and pending until authoritative CI results exist.

Next: resolve actual native build/runtime failures, then integrate Linux native
windows, screen/clipboard/theme services, preload/IPC and distributable packaging.
Windows native renderer/window/sandbox and delivery remain separate required work.
The overall project specification is not complete.

## Native window-probe extension

Keep the public desktop runtime macOS-only while enabling the curated Qt view
on Linux. Separate Mac/AppKit probe/runtime wiring from Linux presentation and
accessibility sources. Reuse upstream Vulkan shader-generation outputs rather
than duplicate custom commands or modify upstream files. An X11-only CI gate
runs real native executables under Xvfb/Openbox, checks page colors in the native
capture as well as input/resize/navigation, and kills the actual renderer child
to verify crash failure. Native Linux acceptance is still pending. This does not
establish a Firefly BrowserWindow API or Wayland/physical-GPU compatibility.
