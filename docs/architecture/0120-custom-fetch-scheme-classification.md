# ADR 0120: Shared fetch classification for registered standard schemes

Status: local macOS native engine/UI classification and existing headless helper
regressions accepted. Custom navigation loading remains unimplemented.

## Problem and decision

The document-bound fetch client requires an existing active custom-origin document.
It cannot bootstrap the first vscode-file:// workbench navigation. Inspection of
the pinned engine shows an earlier requirement: LocalNavigable selects fetch versus
non-fetch navigation with LibWebCommon's is_fetch_scheme. LibWebView's actual
is_url_handled_internally uses the same function before native URL routing. Custom
origin identity alone does not make a registered scheme reach that path.

Compose exactly one guarded addition in the shared C++ fetch classification:
registered standard schemes return true, otherwise execute all upstream code.
Use the same immutable native OriginPolicy already shared by parent and helper.
Keep the upstream FETCH_SCHEMES/HTTP_SCHEMES/LOCAL_SCHEMES constants unchanged;
classification does not make custom URLs HTTP or local URLs. Standard and secure
remain separate flags. Nonstandard or absent registrations gain nothing.

Extend the existing origin/trust/helper-launch composer rather than introduce a
second policy table or modify Ladybird. Require the complete pinned URL.cpp hash,
one exact seam, refusal of missing/duplicate seams, and restoration of every
original byte after removing the owned prefix/addition. Compile this replacement
in LibWebCommon only. Reuse C++ at the existing native predicate boundary with no
new FFI. Mite remains outside the renderer policy graph.

## Evidence and limits

The existing native origin gate now checks actual fetch classification alongside
default/configured/sealed origin/trust policy. Registered standard secure and
insecure schemes are fetch-classified; secure-only nonstandard schemes and unknown
schemes are refused. Native HTTP/local predicates and all builtin fetch schemes
retain their upstream behavior.

The new check-firefly-protocol-routing calls actual LibWebView URL parsing/input
classification and is_url_handled_internally before and after native registration.
Both engine and native UI agree on registered custom URLs including query/fragment,
nonstandard/unknown schemes, HTTPS/HTTP/file/data and mailto. It creates no view,
window, navigation or external-application request. Require it through the existing
fetch gate so macOS release/Linux/experimental Windows workflows exercise it.

Existing actual JavaScript/process resource fetch tests and the real headless
diagnostic/production-helper policy/DOM-origin gate remain separate regression
checks. Only local macOS execution is accepted for the new classification; native
Windows/Linux require fresh hosted results.

Classification is not response authorization. Product helpers still install no
resource provider/client/service, and the scheme dispatcher continues to reject
navigation. This does not implement Electron supportFetchAPI or any public
registration flags. There is no successful custom-origin page load or Code-OSS
workbench launch from this change.

Next implement an explicitly admitted browser-initiated top-level navigation path
before wiring the live helper bootstrap. Bind native expected URL, navigable,
replaced-client and reserved-client scope, and preserve controller cancellation,
response policy and body ownership across document commit. Reusing the existing
document-bound client by relaxing its checks would be incorrect: its source
document becomes inactive on commit, while the navigation body may still be live.
Keep that lifecycle separate from post-commit same-document resource requests.
