# ADR 0086: Native macOS screen snapshots and shared geometry policy

Status: implemented partial slice; local native provider/API-policy validation,
public desktop and physical hotplug acceptance pending.

## Decision

Read display metadata directly from AppKit/CoreGraphics in Objective-C++ under
`firefly/native/screen`. These APIs provide the real menu display, work areas,
backing scale, display IDs and cursor without relying on an opened window.
Convert AppKit bottom-left coordinates to primary-top-left DIP coordinates once
at the boundary. Do not use Qt screen indices as CoreGraphics identifiers or
populate missing Electron color/HDR fields with guessed values.

Expose compact JSON snapshots through two trusted main-process host functions.
The owned ESM module under `firefly/core/screen` implements the EventEmitter,
readiness guards, immutable result snapshots, nearest/intersection policy and
snapshot diffs. The public host module exports the same singleton through ESM
and CommonJS. Mite itself gains no GUI dependency or desktop module.

Qt display-change notifications use the existing Bridge queue and context owner;
they never retain JS pointers. Context teardown cancels callbacks. Commit event
baselines before calling listeners and suppress unchanged snapshots. This
coalescing policy is intentionally not a lossless hardware event stream.

## Validation and limits

A QCoreApplication-only CLI compares actual AppKit records with independent
CoreGraphics display bounds/primary identity and validates work areas/scales.
It creates no QApplication, windows or renderer and never changes cursor state.
The actual API factory runs in both Node and Mite with synthetic negative-origin
layouts, ties, intersection, invalid inputs, empty desktop, mutation/reentrant
events and actual provider records. Synthetic layouts do not certify real
multi-monitor/DPI/rotation behavior.

The shared public Electron/Firefly fixture is queued through the existing hosted
desktop suite, preserving infrequent/batched local GUI acceptance. Native
Windows/Linux backends, full Display records, pixel conversions, hardware hotplug
and notification completeness remain open. No Ladybird changes are required.
