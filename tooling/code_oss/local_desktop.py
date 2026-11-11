"""Exact pinned application adaptations for an opt-in local editor experiment."""

HELPER = "src/vs/base/common/elytraLocalDesktop.ts"
OWNED = "firefly/compat/code-oss/local-desktop.ts"
PATHS = {
    "local-webview-browser.ts": "src/vs/workbench/contrib/webview/browser/webviewService.ts",
    "local-webview-electron.ts": "src/vs/workbench/contrib/webview/electron-browser/webviewService.ts",
    "local-remote-browser.ts": "src/vs/platform/remote/browser/remoteAuthorityResolverService.ts",
    "local-remote-electron.ts": "src/vs/platform/remote/electron-browser/remoteAuthorityResolverService.ts",
    "local-remote-loader.ts": "src/vs/platform/remote/electron-browser/electronRemoteResourceLoader.ts",
    "local-webview-main.ts": "src/vs/platform/webview/electron-main/webviewMainService.ts",
    "local-app-main.ts": "src/vs/code/electron-main/app.ts",
    "local-recording-electron.ts": "src/vs/workbench/contrib/issue/electron-browser/nativeRecordingService.ts",
}
IMPORTS = {
    "mainImpl.ts": "./vs/base/common/elytraLocalDesktop.js",
    **{
        key: "../../../../base/common/elytraLocalDesktop.js"
        for key in ("local-webview-browser.ts", "local-webview-electron.ts")
    },
    **{
        key: "../../../base/common/elytraLocalDesktop.js"
        for key in PATHS
        if not key.startswith("local-webview-b")
        and key not in ("local-webview-electron.ts", "local-recording-electron.ts")
    },
}
# The desktop application sits one directory closer to vs/base than platform services.
IMPORTS["local-app-main.ts"] = "../../base/common/elytraLocalDesktop.js"
IMPORTS["local-recording-electron.ts"] = "../../../../base/common/elytraLocalDesktop.js"
FUNCTIONS = {
    "mainImpl.ts": "assertLocalDesktopCLI, localDesktopCodeCache, localDesktopSchemes",
    "local-webview-browser.ts": "assertLocalDesktopFeature",
    "local-webview-electron.ts": "assertLocalDesktopFeature",
    "local-remote-browser.ts": "rejectLocalDesktopFeature",
    "local-remote-electron.ts": "rejectLocalDesktopFeature",
    "local-remote-loader.ts": "isLocalDesktop, assertLocalDesktopFeature",
    "local-webview-main.ts": "isLocalDesktop, assertLocalDesktopFeature",
    "local-app-main.ts": "isLocalDesktop",
    "local-recording-electron.ts": "isLocalDesktop, assertLocalDesktopFeature, rejectLocalDesktopFeature",
}


def insert(source, seam, extra):
    if source.count(seam) != 1:
        raise ValueError(f"Unexpected local desktop seam: {seam}")
    return source.replace(seam, seam + extra)


def guard_methods(source, prefixes, feature, asynchronous=False):
    for prefix in prefixes:
        starts = [
            line for line in source.splitlines() if line.startswith(prefix) and line.endswith("{")
        ]
        if len(starts) != 1:
            raise ValueError(f"Unexpected feature entry: {prefix}")
        guard = (
            f"\n\t\tconst unavailable = rejectLocalDesktopFeature('{feature}');\n\t\tif (unavailable) {{ return unavailable; }}"
            if asynchronous
            else f"\n\t\tassertLocalDesktopFeature('{feature}');"
        )
        source = insert(source, starts[0], guard)
    return source


def adapt_local(name, data):
    source = data.decode()
    if "elytraLocalDesktop" in source:
        raise ValueError("Duplicate local desktop adaptation")
    # Add an owned leaf policy import, preserving the original license/import bytes.
    seam = next((line for line in source.splitlines() if line.startswith("import ")), None)
    if not seam:
        raise ValueError("No pinned import seam")
    source = insert(source, seam, f"\nimport {{ {FUNCTIONS[name]} }} from '{IMPORTS[name]}';")
    if name == "mainImpl.ts":
        source = insert(source, "const args = parseCLIArgs();", "\nassertLocalDesktopCLI(args);")
        if source.count("const codeCachePath = getCodeCachePath();") != 1:
            raise ValueError("Code cache seam changed")
        source = source.replace(
            "const codeCachePath = getCodeCachePath();",
            "const codeCachePath = localDesktopCodeCache(getCodeCachePath);",
        )
        opening = "protocol.registerSchemesAsPrivileged(["
        if source.count(opening) != 1:
            raise ValueError("Scheme opening seam changed")
        start = source.index(opening)
        end = source.index("\n]);", start)
        source = (
            source[:start]
            + source[start:end].replace(
                opening, "protocol.registerSchemesAsPrivileged(localDesktopSchemes(["
            )
            + source[end:].replace("\n]);", "\n]));", 1)
        )
    elif name.startswith("local-webview-") and name != "local-webview-main.ts":
        prefixes = (
            ["\tcreateWebviewElement("]
            if name.endswith("browser.ts")
            else ["\toverride createWebviewElement("]
        )
        if name.endswith("browser.ts"):
            prefixes.append("\tcreateWebviewOverlay(")
        source = guard_methods(source, prefixes, "webviews")
    elif name in ("local-remote-browser.ts", "local-remote-electron.ts"):
        source = guard_methods(
            source,
            [
                "\tasync resolveAuthority("
                if name.endswith("browser.ts")
                else "\tresolveAuthority(",
                "\tasync getCanonicalURI(",
            ],
            "remote",
            True,
        )
    elif name == "local-remote-loader.ts":
        source = insert(source, "\t\tsuper();", "\n\t\tif (isLocalDesktop()) { return; }")
        source = guard_methods(source, ["\tprivate async doRequest("], "remote resources")
        # Desktop service construction requests these factories even for entirely
        # local windows. Refuse at the actual URI-generation operation, before
        # resource.with can produce an unsupported remote URL.
        for seam, indent in (
            ("\t\treturn (uri: URI) => uri.with({", "\t\t"),
            ("\treturn (resourceId, resource) => resource.with({", "\t"),
        ):
            if source.count(seam) != 1:
                raise ValueError(f"Remote URI provider seam changed: {seam}")
            start = source.index(seam)
            ending = "\n" + indent + "});"
            end = source.index(ending, start)
            head, expression = seam.split("=> ", 1)
            guarded = (
                head
                + "=> {\n"
                + indent
                + "\tassertLocalDesktopFeature('remote resources');\n"
                + indent
                + "\treturn "
                + expression
            )
            source = (
                source[:start]
                + source[start:end].replace(seam, guarded, 1)
                + "\n"
                + indent
                + "\t});\n"
                + indent
                + "};"
                + source[end + len(ending) :]
            )
    elif name == "local-webview-main.ts":
        seam = "\t\tthis._register(instantiationService.createInstance(WebviewProtocolProvider));"
        if source.count(seam) != 1:
            raise ValueError("Webview initialization seam changed")
        source = source.replace(seam, "\t\tif (!isLocalDesktop()) {\n" + seam + "\n\t\t}")
        source = guard_methods(
            source,
            [
                "\tpublic async setIgnoreMenuShortcuts(",
                "\tpublic async findInFrame(",
                "\tpublic async stopFindInFrame(",
            ],
            "webviews",
        )
    elif name == "local-recording-electron.ts":
        source = insert(
            source,
            "\tget isSupported(): boolean {",
            "\n\t\tif (isLocalDesktop()) { return false; }",
        )
        source = guard_methods(
            source,
            ["\tasync startRecording(", "\tasync openScreenCapturePermissionSettings("],
            "display capture",
        )
        source = guard_methods(
            source, ["\trequestScreenCapturePermission("], "display capture", True
        )
    elif name == "local-app-main.ts":
        start = (
            "\t\tconst remoteResourceChannel = new Lazy(() => mainProcessElectronServer.getChannel("
        )
        end = "\t\t\turl => remoteResourceChannel.value.call<NodeRemoteResourceResponse>(NODE_REMOTE_RESOURCE_IPC_METHOD_NAME, [url])\n\t\t);"
        if source.count(start) != 1 or source.count(end) != 1:
            raise ValueError("Remote initialization seam changed")
        source = source.replace(start, "\t\tif (!isLocalDesktop()) {\n" + start).replace(
            end, end + "\n\t\t}"
        )
        # The same disabled remote feature must not register a fetch route after
        # window creation when its scheme was deliberately omitted at startup.
        remote_handler = (
            "\t\tprotocol.handle(Schemas.vscodeRemoteResource, createRemoteResourceRequestHandler(this.logService));\n"
            "\t\tthis._register(toDisposable(() => protocol.unhandle(Schemas.vscodeRemoteResource)));"
        )
        if source.count(remote_handler) != 1:
            raise ValueError("Remote protocol registration seam changed")
        source = source.replace(
            remote_handler, "\t\tif (!isLocalDesktop()) {\n" + remote_handler + "\n\t\t}"
        )
        # Only this optional capture registration is deferred. Security permission
        # callbacks and all webRequest filtering below remain unchanged.
        display_start = "\t\tsession.defaultSession.setDisplayMediaRequestHandler(async (request, callback) => {"
        display_end = "\t\t});\n\n\t\t//#endregion\n\n\t\t//#region Request filtering"
        if source.count(display_start) != 1 or source.count(display_end) != 1:
            raise ValueError("Display capture registration seam changed")
        source = source.replace(
            display_start, "\t\tif (!isLocalDesktop()) {\n" + display_start
        ).replace(display_end, display_end.replace("\t\t});", "\t\t});\n\t\t}", 1))
    else:
        raise ValueError(f"Unknown local adaptation: {name}")
    return source.encode()
