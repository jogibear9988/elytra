# ADR 0187: Disabled native spelling admission

Status: implemented subset; platform and renderer limits remain in PROGRESS.

Code-OSS requests `spellcheck:false`. This preference affects actual renderer
behavior and must not be removed as if it were a V8 engine tuning option.
Firefly's Qt application does not install a WebView::SpellChecker. Upstream
ViewImplementation::send_preferences_to_page forwards this real provider's
availability to each page, including new helpers. Missing providers also produce
empty spelling and writing-suggestion responses in the native request handlers.
No downstream Ladybird modification is required for this disabled state.

The shared BrowserWindow/WebContentsView validator accepts only boolean false,
only when a trusted native query observes the provider is actually absent. True
is explicitly unsupported. Both native creation bindings independently enforce
provider absence before preload I/O and allocation, including default requests.
A future provider integration therefore fails closed until actual per-view policy
and request handling are implemented. A one-off page availability override would
be insufficient because upstream resends preferences during page/helper lifecycle.

Omission currently means disabled, unlike Electron's enabled default. Session
language/dictionary/provider controls and enabled spellchecking remain unfinished.
Do not infer backend availability from DOM spellcheck attributes: those reflect
author intent and can remain true while native checking is disabled.

Tests compare the actual JS policy under Node/Mite, load real documents with
hidden windows/views under Firefly/Electron, and install/remove a genuine native
provider in an exact-production-object offscreen frontend. The native frontend
checks unchanged widget counts when both explicit and omitted requests reject,
then successful recovery after provider removal. These tests do not exercise a
renderer spelling RPC, OS dictionaries, underline rendering or fresh platforms.
