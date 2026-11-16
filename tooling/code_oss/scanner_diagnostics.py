"""Historical CA-29 transform, retained only for exact retirement verification.

The active overlay no longer imports this module or exposes its option.
"""

import hashlib

SOURCE = "src/vs/platform/extensionManagement/common/extensionsScannerService.ts"
SOURCE_SHA256 = "572182f1b3a3f10b2301d645bbaebfde27dbfc215d8e22b1f464f16339d78f48"
HELPER_SOURCE = "firefly/compat/code-oss/scanner-diagnostics.ts"
HELPER_TARGET = "src/vs/platform/extensionManagement/common/elytraScannerDiagnostics.ts"
IMPORT = "import { coalesce } from '../../../base/common/arrays.js';"
ADDED_IMPORT = "import { scanBegin, scanEnd } from './elytraScannerDiagnostics.js';\n"
# Each complete original statement remains byte-identical, between observations.
SEAMS = (
    (
        "input-translations",
        "\t\tconst translations = await this.getTranslations(language ?? platform.language);",
    ),
    ("input-mtime", "\t\tconst mtime = await this.getMtime(location);"),
    (
        "input-profile-mtime",
        "\t\tconst applicationExtensionsLocationMtime = applicationExtensionsLocation ? await this.getMtime(applicationExtensionsLocation) : undefined;",
    ),
    ("stat", "\t\t\tconst stat = await this.fileService.stat(location);"),
    ("directory", "\t\tconst stat = await this.fileService.resolve(input.location);"),
    (
        "manifest",
        "\t\t\tcontent = (await this.fileService.readFile(manifestLocation)).value.toString();",
    ),
    (
        "translate",
        "\t\t\tmanifest = await this.translateManifest(input.location, manifest, ExtensionScannerInput.createNlsConfiguration(input));",
    ),
    (
        "nls-translation",
        "\t\t\t\tconst content = (await this.fileService.readFile(translationResource)).value.toString();",
    ),
    ("nls-exists", "\t\t\tconst exists = await this.fileService.exists(defaultPackageNLS);"),
    (
        "nls-find",
        "\t\t\t\tmessageBundle = await this.findMessageBundles(extensionLocation, nlsConfiguration);",
    ),
    (
        "nls-localized",
        "\t\t\t\tconst messageBundleContent = (await this.fileService.readFile(messageBundle.localized)).value.toString();",
    ),
    (
        "nls-original",
        "\t\t\t\tconst originalBundleContent = (await this.fileService.readFile(originalMessageBundle)).value.toString();",
    ),
    (
        "profile-scan",
        "\t\tconst scannedProfileExtensions = await this.extensionsProfileScannerService.scanProfileExtensions(profileResource, input.profileScanOptions);",
    ),
    ("cache-read", "\t\t\tconst cacheRawContents = await this.fileService.readFile(cacheFile);"),
    (
        "cache-write",
        "\t\t\tawait this.fileService.writeFile(cacheFile, VSBuffer.fromString(JSON.stringify(cacheContents)));",
    ),
)


def replacement(index, phase, original):
    indent = original[: len(original) - len(original.lstrip("\t"))]
    return (
        f"{indent}const elytraScan{index} = scanBegin('{phase}');\n"
        + original
        + f"\n{indent}scanEnd('{phase}', elytraScan{index});"
    )


def adapt_scanner(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Extension scanner differs from pinned original input")
    text = data.decode("utf-8")
    if text.count(IMPORT) != 1 or "elytraScannerDiagnostics" in text:
        raise ValueError("Unexpected scanner diagnostic import")
    text = text.replace(IMPORT, ADDED_IMPORT + IMPORT)
    for index, (phase, original) in enumerate(SEAMS):
        if text.count(original) != 1:
            raise ValueError("Unexpected scanner diagnostic seam: " + phase)
        text = text.replace(original, replacement(index, phase, original))
    result = text.encode("utf-8")
    if revert_scanner(result) != data:
        raise ValueError("Scanner diagnostic reversal differs")
    return result


def revert_scanner(data):
    text = data.decode("utf-8")
    if text.count(ADDED_IMPORT) != 1:
        raise ValueError("Missing/duplicate scanner diagnostic import")
    text = text.replace(ADDED_IMPORT, "")
    for index, (phase, original) in enumerate(SEAMS):
        added = replacement(index, phase, original)
        if text.count(added) != 1:
            raise ValueError("Missing/duplicate scanner diagnostic phase: " + phase)
        text = text.replace(added, original)
    result = text.encode("utf-8")
    if hashlib.sha256(result).hexdigest() != SOURCE_SHA256:
        raise ValueError("Scanner diagnostic original drift")
    return result
