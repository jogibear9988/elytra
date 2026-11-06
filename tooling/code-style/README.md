# Source style tooling

These pinned development tools format maintained sources; neither runtime needs
Node, Python formatters or clang-format to execute an application.

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/code-style
python3 -m venv build/code-style-venv
build/code-style-venv/bin/pip install -r tooling/code-style/requirements.txt
python3 tooling/code_style.py
python3 tooling/code_style.py --check
```

Use two spaces for JS/web/config sources and four for native/Python sources.
Functions and methods have separate lines and a blank line between definitions.
Comments explain purpose, ownership, cancellation/error rules and compatibility
choices. Avoid comments that merely repeat a method name or individual statement.

The JS formatter uses pinned Prettier followed by Acorn-based definition spacing.
Native formatting uses the pinned clang-format profile; native.py supplies blank
lines around completed definitions/declarations while preserving literal contents.
Python uses Ruff's formatter, which provides normal method/function separation.

Generated runtime bundles have their own reproducible generator and integrity
manifest. Vendor code and deliberately authored parser/module fixtures are outside
automatic rewriting: their source bytes or module format can be the subject of a
test. Explain an exception next to the fixture or in the corresponding suite.
