# Regression command-line tools

Tests are kept separate from build, packaging and integrity entrypoints. Each test
remains directly runnable, for example:

```sh
python3 tooling/tests/test_crypto_hmac.py --mite build/release-mite/bin/mite
python3 tooling/tests/test_firefly.py --firefly build/renderer/bin/firefly --electron build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron
```

_support.py supplies one explicit import-path setup for these standalone CLIs.
Sibling helpers, native subprocesses, temporary fixtures and their lifetimes are
owned by each suite. CMake/CI call the same scripts and retain their existing test
names. Directory placement does not establish platform or API acceptance.

CommonJS fixtures deliberately exercise CommonJS compatibility; ordinary fixture
sources still follow the formatting rules. Four malformed JS/ESM/JSON inputs have
specific exceptions because parser rejection is the tested behavior.
