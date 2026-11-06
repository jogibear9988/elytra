# ADR 0088: Native theme policy and existing Ladybird appearance controls

Status: partial macOS implementation; windowless native/policy and targeted
Firefly/Electron CSS acceptance pass, wider physical/visual/platform tests open.

## Decision

Implement nativeTheme as an owned ESM EventEmitter with lazy OS snapshots,
application-local system/light/dark selection, getter-only state and queued
snapshot comparisons. Read actual macOS global dark and accessibility preferences
in Objective-C++ without writing OS defaults. Override selection stays separate
from the system preference and shared across the application's views.

Reuse upstream Settings::set_appearance for interface/website preferences and
SystemAppearance::apply_interface_theme for Qt. Existing public LibWebView APIs
deliver preferred-color-scheme changes to renderer processes; no Ladybird source
patch, copied browser frontend or DOM/CSS substitution is required. Firefly uses
a temporary profile, so persistence affects its own profile only.

Connect upstream appearance and native NSWorkspace accessibility notifications
through the Bridge queue. Native observers retain no JS objects; context teardown
removes observers and QPointer guards invalidate already queued callbacks.
Commit JS baselines before listeners and coalesce unchanged observations.

## Evidence and limits

A QCoreApplication-only probe reads OS flags and verifies in-process native
notification registration/removal without changing a preference or opening a
window. The actual API factory runs in Node and Mite with fixtures and native
records. Targeted actual Firefly/Electron fixtures exercise override selection
and current CSS queries in existing/new hidden windows.

The initial event-only hidden-window test did not observe a light transition;
independent current matchMedia queries establish actual preference delivery.
Hidden rendering-driven event timing is not claimed. The Electron reference
also waits for ESM entry evaluation before ready: a top-level await on readiness
created a startup cycle. Shared fixtures now stage static desktop imports and
finish module evaluation before whenReady().then callbacks; assertions are kept.

Full visible native chrome, physical OS/accessibility notification changes,
contrast rendering, exact Electron event counts/coercions and Windows/Linux
desktop remain open. Do not infer these from synthetic events or query flags.
