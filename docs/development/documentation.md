# Maintain the documentation

The [Node/Electron baseline](../reference/documentation-baseline.md) defines the
breadth and reference quality we aim for. The [roadmap](../roadmap.md) separates
missing implementation from missing documentation. Documents must describe the
current checkout, with evidence and limitations, rather than the desired final API.

## Put information where readers expect it

Use docs/index.md as the entry point. Guides teach a task; concepts explain the
runtime model; API entries define contracts; migration pages identify changes and
blockers. Compatibility matrices record measured scope. Development pages explain
build/release/contribution workflows; ADRs preserve architectural decisions.

Link between these layers. A new API should have a guide/example when it enables a
useful task, a reference entry and a compatibility row. An internal prototype belongs
in development/architecture, not a public application tutorial.

## Reference acceptance

State implementation status, tested platforms, process/realm, parameters/defaults,
supported options, returns, errors, async/event behavior and resource lifetime.
Include a small example and exact source/test links. Explain unsupported modes and
intentional differences. Separate synchronous throws from Promise rejection when
the distinction affects callers. Do not infer Electron or Node behavior from a
matching method name.

Use actual version/change notes when a behavior changes. Do not invent Added in
release tags, stability guarantees, LTS schedules or platform support. Source snapshots
and local test results are not published binary releases.

## Keep evidence together

In each coherent implementation commit update the relevant reference, measured
matrix, runnable example/test and docs/PROGRESS.md. Record what was implemented,
what was tested and what remains. If only documentation changes, run link/build
checks and execute new recipes where possible; do not claim runtime validation from
a documentation build.

For externally inspired guidance, link the primary documentation and write our own
explanation. Preserve licenses for assets or redistributed code. Avoid copying
reference-project branding or unsupported API examples into Firefly tutorials.

## Build and review

From the repository root:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/site
node tooling/site/build.mjs
python3 tooling/check_site.py build/site
```

Check rendered desktop/mobile pages, navigation, outlines, code and tables. Source
Markdown links must remain usable on GitHub; the builder rewrites local Markdown to
published HTML and source-code links to master. New top-level documentation sections
should also be added to the grouped navigation in tooling/site/build.mjs.

[Website development](website.md) explains publishing/assets. The current site is a
single snapshot, not a version selector. Versioned reference archives/search and
complete per-method runtime reference remain explicit documentation work.
