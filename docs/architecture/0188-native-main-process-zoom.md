# ADR 0188: Main-process browser zoom shares native view state

Status: implemented subset, with explicit per-view/lifecycle differences.

Code-OSS's first-window preferences include zoomFactor. Preserve the actual
behavior rather than removing the preference as engine tuning. Main-process
factor/level methods and properties, BrowserWindow/WebContentsView constructors
and existing isolated-preload webFrame use ViewImplementation's real zoom state.
There is no JS/native mirrored factor. Native setters propagate through upstream
update_zoom to current hosting pages; send_preferences_to_page covers new hosts.

Use strict finite numeric primitives and the pinned native 0.3..5 range, refusing
out-of-range values before upstream's clamp. Levels use factor=1.2^level. Both JS
policy and native entry points validate without coercing user objects. Factory
validation occurs before preload I/O and widget allocation; initial application
of the factor occurs before showing or loading the view.

The first actual renderer test disproved relying on set_zoom alone: initial
content_settings_changed reset the constructor factor to the browser default.
An owned exact-source composition now bypasses only apply_zoom_for_current_host
for registered Firefly views. Unowned upstream views retain their full original
body. A bounded UI-thread registry holds only membership, with QObject destruction
removing an opaque base pointer after base teardown. Original native zoom remains
the single source of truth, including subsequent preload setters. The canvas and
zoom compositions reverse together to the exact pinned upstream bytes and refuse
changed, missing or duplicated seams before any output write. Ladybird stays clean.

Do not reuse upstream Settings::set_zoom_for_host as Electron origin/session
policy: its host-only keys omit scheme, port and session, omit file hosts, persist
global settings and notify every browser view. Our current per-webContents scope
matches the existing preload subset. Full origin/session sharing and persistence
need their own implementation. Firefly preserves current zoom across tested
navigation/reload; Electron 45.2.3 reapplies the constructor preference. Constructor
getter timing also differs. These are explicit limits, not full compatibility.

Node/Mite policy and exact-source tests pass. Hidden Firefly/Electron tests measure
actual innerWidth and devicePixelRatio, compare preload/main getters, exercise
runtime setters/properties and navigation/reload with their respective documented
policies. A child view verifies native independence. Existing preload zoom and
seven real CPU canvas stages remain green. Fresh platforms, physical input/GPU
presentation, origin/session sharing and visual zoom limits remain open.
