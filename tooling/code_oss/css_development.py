"""Exact local-only CSS inventory branch before native ripgrep discovery."""

import hashlib

SOURCE = "css-development-service.ts"
SOURCE_SHA256 = "3b6c452087f79f3aafbde34285fd94f6c5d69730522441ad362d989bcda088b6"
HELPER_SOURCE = "firefly/compat/code-oss/css-development.ts"
HELPER_TARGET = "src/vs/base/node/elytraCSSDevelopment.ts"
IMPORT = "import { spawn } from 'child_process';"
ADDED_IMPORT = (
    "\nimport { libJSDevelopmentCSSModules } from '../../../base/node/elytraCSSDevelopment.js';"
)
SEAM = "\t\tconst rgBinPath = await rgDiskPath();"
BRANCH = "\t\tconst stagedCSS = libJSDevelopmentCSSModules(FileAccess.asFileUri('').fsPath);\n\t\tif (stagedCSS !== undefined) { return await stagedCSS; }\n\n"


def adapt_css_development(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS CSS development service differs from pinned input")
    source = data.decode("utf-8")
    if source.count(IMPORT) != 1 or source.count(SEAM) != 1:
        raise ValueError("Unexpected Code-OSS CSS discovery seam")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(SEAM, BRANCH + SEAM).encode()


def revert_css_development(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(BRANCH + SEAM) != 1:
        raise ValueError("Unexpected adapted Code-OSS CSS discovery seam")
    original = source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(BRANCH + SEAM, SEAM).encode()
    if adapt_css_development(original) != data:
        raise ValueError("Adapted Code-OSS CSS discovery drifted")
    return original
