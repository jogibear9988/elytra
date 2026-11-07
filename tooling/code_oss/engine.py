"""Exact insertion seams for the pinned Code-OSS engine-only default tuning block."""

IMPORT = "import { app, protocol, crashReporter, Menu, contentTracing } from 'electron';"
ADDED_IMPORT = "\nimport { isLibJSHost, assertLibJSStartupOptions, recordLibJSDefaults } from './elytraEnginePolicy.js';"
CONFIG = "\tconst argvConfig = readArgvConfigSync();"
CHECK = "\n\tassertLibJSStartupOptions(cliArgs, argvConfig, app.commandLine);"
START = "\t// Following features are enabled from the runtime:"
OPEN = "\tif (isLibJSHost()) {\n\t\trecordLibJSDefaults();\n\t} else {\n"
END = "\treturn argvConfig;\n}\n\ninterface IArgvConfig"
CLOSE = "\t}\n\n"


def adapt_engine(data):
    """Preserve all original bytes; wrap only audited implicit engine defaults."""
    source = data.decode("utf-8")
    for seam in (IMPORT, CONFIG, START, END):
        if source.count(seam) != 1:
            raise ValueError(f"Unexpected Code-OSS engine configuration seam: {seam}")
    # Ensure the wrap cannot accidentally move before user settings/after return.
    if not source.index(CONFIG) < source.index(START) < source.index(END):
        raise ValueError("Code-OSS engine block order changed")
    return (
        source.replace(IMPORT, IMPORT + ADDED_IMPORT)
        .replace(CONFIG, CONFIG + CHECK)
        .replace(START, OPEN + START)
        .replace(END, CLOSE + END)
        .encode("utf-8")
    )
