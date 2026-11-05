# Website and API documentation

The project website is published at https://jogibear9988.github.io/elytra/.
Its sources are site/ and the repository's Markdown docs, including the API reference.
Node/npm are build-time tools only. The generated website has no server, external
fonts, framework runtime or analytics.

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/site
node tooling/site/build.mjs
python3 tooling/check_site.py build/site
python3 -m http.server 8000 --directory build/site
```

Open http://localhost:8000/. The build renders Markdown with pinned markdown-it,
rewrites local documentation links, provides heading anchors and checks local pages,
resources, fragments and relative project-Page paths. Generated files stay under build/.
The homepage's tabs support click, arrows, Home/End and reduced-motion preferences;
content and navigation remain available without JavaScript.

The Project website Actions workflow builds on documentation/site changes to main or
the active development branch. Pull requests build/check only. Pushes deploy with
GitHub's Pages artifact/OIDC mechanism; no PAT is stored in the workflow. GitHub Pages
must use the GitHub Actions source and the github-pages environment must permit the
publishing branch. Branch deployment rules remain GitHub's protection boundary.

When the implementation moves to main, remove the development branch from the push
trigger and change the generator's source-link ref/getting-started checkout command.
Update API prose and compatibility matrices with implementation commits. Do not
promote planned Windows/Linux, public IPC or general compatibility into supported
features based only on website/release infrastructure.

The visual direction takes inspiration from the developer-focused project pages of
[Electron](https://www.electronjs.org/), [Node.js](https://nodejs.org/en) and
[Bun](https://bun.sh/): clear examples, product explanations and direct documentation
links. The styling, graphics and wording here are original; there is no affiliation.
