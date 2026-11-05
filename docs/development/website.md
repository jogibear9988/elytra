# Website and API documentation

The project website is published at https://jogibear9988.github.io/elytra/.
Its sources are site/ and the repository's Markdown docs, including the API reference.
Node/npm are build-time tools only. The generated website has no server, external
font requests, framework runtime or analytics.

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/site
node tooling/site/build.mjs
python3 tooling/check_site.py build/site
python3 -m http.server 8000 --directory build/site
```

Open http://localhost:8000/. The build renders Markdown with pinned markdown-it,
rewrites local documentation links, provides heading anchors and checks local pages,
resources, fragments and relative project-Page paths. CSS, JS and favicon URLs
are content-addressed so a browser cannot mix the new layout with cached old assets. Generated files stay under build/.
The homepage's example tabs support click, arrows, Home/End and reduced-motion
preferences. Copy controls use the Clipboard API and select the code on denial.
Documentation/navigation and the first example remain available without JavaScript.

## Visual direction

The homepage was rebuilt after the first layout was rejected. Its structure now
centers the two product wordmarks and original illustrated Mite/Firefly characters,
then presents a full-width code workbench, ruled project entries and a real Designer
compositor capture. Paper/ink colors, green/yellow product accents and self-hosted
Bricolage Grotesque / IBM Plex typography carry through the API reference.

[Node.js](https://nodejs.org/en), [Bun](https://bun.sh/) and
[Electron](https://www.electronjs.org/) are the requested visual references. Their
actual homepage captures were inspected during this redesign. Artwork, layout and
wording are original; there is no affiliation. The Designer capture is our actual
LibWeb output from the pinned MIT-licensed upstream demo, not a fabricated app mockup.

Fonts come from version 5.3.0 Fontsource packages; files, checksums and preserved OFL
notices are under site/fonts. They are site assets, not Mite/Firefly runtime dependencies.
The two mascots and favicon are small, owned SVGs rather than external icon/font assets.

## Documentation navigation

The Docs header opens docs/index.md. The builder maintains grouped Start, Concepts,
API, Application guides, Status and Development navigation, expands the current
page's category and marks its link with aria-current. Native details controls keep
navigation usable without JavaScript. On small screens, site.js initially collapses
the menu and page outline; either can be opened with keyboard/touch.

Page outlines are generated from h2/h3 headings and use the same fragment IDs as
the content. They sit alongside desktop articles and become an inline control at
narrower widths. Add new learning/reference sections to the groups in the builder
and cross-link them from the documentation home. The checker validates generated
navigation and outline destinations along with source links/assets.

## Publishing

Work is committed and pushed directly to master at the user's request; no pull
request is required. The Project website Actions workflow deploys site/docs changes
from master using GitHub Pages artifact/OIDC. Other branches/PRs do not publish the
production site. PRs can still run the build/check job if used by future contributors.
The github-pages environment must explicitly permit master. No PAT is stored in
the workflow. Source links and getting-started checkout commands use master.

Update API prose and compatibility matrices with implementation commits. Do not
promote planned Windows/Linux, public IPC or general compatibility into supported
features based only on website/release infrastructure.
