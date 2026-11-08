# Application locale and system languages

Status: partial main-process API, accepted locally on macOS Release. The real
Code-OSS main source needs these methods for language recommendations and NLS.
They expose native regional/language settings and the selected engine language;
Chromium translation packs and full Code-OSS startup are not supplied.

```js
import {app} from 'firefly';

// These reads are available before readiness and do not create a window.
const preferred = app.getPreferredSystemLanguages();
const country = app.getLocaleCountryCode();
app.commandLine.appendSwitch('lang', 'de-DE');

await app.whenReady();
console.log({preferred, country,
  application: app.getLocale(),
  regional: app.getSystemLocale()
});
```

All methods are synchronous, take no parameters and ignore extra JavaScript
arguments. They share the same app singleton through ESM, CommonJS and the explicit
--electron-compat alias. Reads do not modify system settings or open a window.
Only trusted main code has the native binding; nothing is added to page/preload
capabilities. Standalone Mite does not depend on these Qt/Foundation services.

## app.getPreferredSystemLanguages()

Returns a fresh mutable string array, in native preference order. Available before
readiness. On macOS, read NSLocale.preferredLanguages directly, preserving its
language/script/region identifiers and order. Do not manufacture country codes
for missing regions. Mutating the returned array cannot alter the operating system
or subsequent results. Read again to obtain the current native preference list;
there is no language-change event in this slice.

On Linux, the Qt system uiLanguages list includes equivalent tags and fallback
forms. Its shape differs from Electron's GLib list; this remains a documented
partial implementation, pending fresh hosted Linux acceptance. POSIX C returns an
empty preferred-language array. No Windows desktop provider is implemented.

## app.getSystemLocale()

Returns the native regional locale string. Requires app readiness; before it,
throw ERR_FIREFLY_NOT_READY. The native boundary independently repeats the check.
This reflects regional formatting preferences, not the first preferred UI language
and not the --lang override.

On macOS, read NSLocale.currentLocale and its canonical language identifier. On
Linux, use Qt's system language/territory name with dashes; C is reported explicitly
as en-US-POSIX. Qt's language/territory form is not complete Chromium/ICU identifier
parity. If no native locale is available, throw ERR_FIREFLY_UNSUPPORTED_API instead
of returning a fabricated configured locale.

## app.getLocale()

Returns the first language in the **actual initialized WebView settings**. Requires
readiness and is available inside ready listeners. This is the engine/application
language selection, not a Chromium l10n_util translation-pack identifier. For
example, a de-DE override returns de-DE; do not assume Electron's shorter de alias.
Firefly does not currently ship translated application UI through this method.

The owned application now supplies native preferred languages to upstream's public
platform hook. Upstream filters unavailable languages, chooses its default/fallback
and loads persisted profile preferences. An explicit early --lang or
app.commandLine.appendSwitch('lang', tag) then selects a supported canonical BCP 47
language. Removing that switch before readiness restores the loaded profile list.
See [commandLine](command-line.md) for validation, mutation and timing errors.
No new live language setter or post-ready mutation is supplied here.

System language/locale reads remain independent of this selection. If no selected
engine language exists, the native binding throws ERR_FIREFLY_UNSUPPORTED_API.
The API cannot be used before initialization to guess what a stored profile will
select. General Chromium locale negotiation and exact translation-pack mapping
remain unimplemented.

## app.getLocaleCountryCode()

Returns the native regional two-letter country code, or the empty string when
unavailable. Available before readiness. It comes from NSLocale's country field on
macOS and Qt's system territory on Linux; it is not inferred from --lang or from the
first preferred language. Numeric/missing territories do not become invented
ISO country codes. Linux C has no country preference and returns the empty string.

## Validation and limits

```sh
cmake --build build/renderer --target check-firefly-app-locale --parallel 3
```

The gate checks actual LibJS/native argument/readiness enforcement without a GUI,
fresh arrays and public host singleton/readiness behavior. macOS public acceptance
uses a separate Foundation reference process and process-local AppleLanguages /
AppleLocale settings. French/Canada, English/Finland, German/Serbian application
overrides and Japanese stored profile language deliberately differ. No OS user
defaults are written by the fixture. It covers launch/append/remove language switches,
ready listeners, fresh default profile selection and zero constructed windows.

The existing targeted command-line hidden-renderer gate additionally checks that
app.getLocale and navigator.languages agree for the selected language. The locale
gate itself never creates a window. Linux CI requires the same host gate with
additional fr_CA/C environment cases; fresh hosted execution remains required.
Native Windows desktop and exact Electron locale-string parity remain unfinished.
The full Code-OSS entry is still blocked earlier by allowServiceWorkers.

See [ADR 0135](../architecture/0135-native-application-locale.md).
Baseline: [Electron app language APIs](https://www.electronjs.org/docs/latest/api/app#appgetpreferredsystemlanguages)
and [Qt locale language fallback rules](https://doc.qt.io/qt-6/qlocale.html#uiLanguages).
