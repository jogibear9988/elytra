"""Guard disabled Darwin updater subscriptions only for the local LibJS application."""

import hashlib

SOURCE = "darwin-update-service.ts"
SOURCE_SHA256 = "8eead08ba810059a02dfeaf24e09ba69b45ef987b06e0e6680931c210cc5fd6b"
IMPORT = "import * as electron from 'electron';"
ADDED_IMPORT = "\nimport { isLocalDesktop } from '../../../base/common/elytraLocalDesktop.js';"
SEAM = "\t\tawait super.initialize();"
GUARD = """

        if (isLocalDesktop()) {
            if (typeof process.versions.mite !== 'string' || typeof process.versions.node === 'string' || typeof process.versions.electron === 'string' || this.state.type !== StateType.Disabled) {
                throw Object.assign(new Error('Native Darwin update installation is unsupported in the local LibJS application'), { code: 'ERR_CODE_OSS_LOCAL_UPDATER_UNSUPPORTED' });
            }
            this.logService.info('Local LibJS: native updater subscriptions skipped because updates are disabled');
            return;
        }""".replace("    ", "\t")


def adapt_disabled_updater(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS Darwin update service differs from pinned input")
    source = data.decode("utf-8")
    if source.count(IMPORT) != 1 or source.count(SEAM) != 1:
        raise ValueError("Unexpected Darwin updater initialization seam")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(SEAM, SEAM + GUARD).encode()


def revert_disabled_updater(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(SEAM + GUARD) != 1:
        raise ValueError("Unexpected adapted Darwin updater initialization")
    original = source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(SEAM + GUARD, SEAM).encode()
    if adapt_disabled_updater(original) != data:
        raise ValueError("Adapted Darwin updater initialization drifted")
    return original
