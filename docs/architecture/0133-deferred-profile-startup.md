# ADR 0133: Select real application/session paths before opening the profile

Status: accepted for native app paths and default-profile persistence.

## Context

The real Code-OSS entry now reaches app.setPath('userData', ...). Previously
Firefly created WebView::Application, a temporary profile, memory-only browser
storage and native services before the Mite entry. Merely returning an overridden
path would leave browser state attached to the old profile and would not fulfill
the intended path/session behavior.

## Decision

Construct the owned WebView application and Qt/Core event loop, then run the main
Mite bootstrap before calling upstream's protected initialize API. Install only
safe early main services. Hold language/theme settings until the selected profile
exists. Keep the launch-only CPU option validated separately. At readiness, seal
real AppPaths, initialize upstream with an actual --profile-path, activate queued
settings and then deliver ready. Use upstream SQL-backed default stores.

All changes remain in owned adapters; no private-header trick, upstream patch or
replacement browser storage is needed. Keep native argv storage alive throughout
the application, and keep QApplication alive until after WebView's helper/session/
database destructor. An early explicit exit skips profile startup. Initialization
errors fail without a ready event or removal of existing user files.

Paths independently validate real directories. userData supplies the default
sessionData; explicit sessionData/log overrides remain independent. Freeze live
profile paths at initialization instead of rebinding open stores. Default identity
uses bounded package metadata or an entry-directory hash to separate apps. Keep
crashDumps' existing process-lifetime contract independent. Expose the actual main
filesystem locations, getAppPath and log-directory creation, with explicit errors
for other path/identity APIs.

## Acceptance and remaining work

Native tests cover real filesystem paths/Unicode, session inheritance, rollback,
independent overrides, logs, executable lookup and sealing. Public acceptance uses
two launches against one HTTP origin and the selected profile: real cookie/local-
storage state survives, native settings/SQLite files live under sessionData, and
application files live under userData. Early language/theme and actual owned-renderer
OS sandbox checks pass on macOS. A conflicting config file fails initialization,
preserves that file and does not emit ready. Targeted command-line, crash reporting
and public protocol regressions exercise the changed bootstrap boundary.

This does not establish native Windows desktop or fresh Linux acceptance, all
storage APIs, concurrent use/locking semantics, Chromium database migration or
profile recovery guarantees. Existing temporary data is not migrated. The real
Code-OSS probe with its selected existing userData now advances to unsupported
protocol privilege corsEnabled (exit 1); workbench loading is still unfinished.
See the [path contract](../api/app-paths.md) and current PROGRESS evidence.
