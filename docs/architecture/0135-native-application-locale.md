# ADR 0135: Expose native regional/UI preferences and the selected engine language

Status: accepted for the tested macOS main-process locale slice.

## Context

The pinned Code-OSS mainImpl uses getPreferredSystemLanguages before app readiness,
getSystemLocale after readiness and getLocale for language configuration. These
are independent OS/application values. Returning the same --lang value for each
would configure regional formatting incorrectly and misrepresent native preferences.
The owned WebView application also inherited an empty platform language hook,
which meant fresh profiles did not start from the actual native preferred language.

## Decision

Use a small main-only native locale provider and guarded LibJS binding. Read
Foundation directly on macOS, preserving the OS preferred-language list rather than
Qt's expanded translation lookup fallbacks. Use Qt's native system locale on Linux,
explicitly documenting its language-list/name differences and unverified hosted
acceptance. Keep Windows unsupported until its desktop provider is implemented.
No new runtime library or upstream patch is required.

Supply native preferred languages through WebView's existing protected virtual
hook. Preserve upstream supported-language filtering and persisted profile loading.
Expose the first actual initialized engine language as getLocale. Keep this separate
from native regional locale, native country and the fresh ordered UI-language array.
The existing early command-line adapter still applies/validates --lang; removal
restores profile/default selection. Do not invent Chromium translation packs or
claim exact Electron locale aliases from LibWeb language settings.

The main lifecycle supplies native readiness only after profile initialization and
engine-language activation, before ready callbacks. Both public factory and native
binding reject application/system locale reads before readiness. Preferred languages
and country are available early. Fresh VM-owned arrays contain no borrowed native
objects and cannot mutate preferences. No renderer/preload or independent Mite
capabilities change.

## Acceptance and remaining work

Direct LibJS/native tests exercise selectors, malformed values, readiness, fresh
arrays and separate application-language callback data. Public real-host acceptance
uses no windows. A separate Foundation oracle and process-local native preferences
verify deliberately different UI languages, regional locale/country and application
selection. Launch/append/remove --lang, stored profile restoration, new-profile
OS selection and ready-listener timing are covered. The existing batched hidden
command-line renderer test ties getLocale to actual navigator.languages.

Local macOS Release passes. Required hosted Linux fr_CA/C checks are wired but not
yet accepted; native Windows desktop remains absent. Complete Electron string
mapping, dynamic language notifications, UI translation packs and Code-OSS workbench
startup are not claimed. The actual earlier Code-OSS worker/cache/remote-scheme
contracts remain separate blockers; this implements independently required APIs.
