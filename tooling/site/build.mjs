/**
 * Compile repository documentation, rewrite source links and fingerprint static site assets.
 */
import { readFile, writeFile, mkdir, cp, readdir } from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import MarkdownIt from 'markdown-it';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = path.resolve(root, process.argv[2] || 'build/site');
if (
  out === root ||
  !out.startsWith(root + path.sep) ||
  out.startsWith(path.join(root, 'vendor') + path.sep)
) {
  throw Error('Site output must be a separate directory in this checkout');
}
const repo = 'https://github.com/jogibear9988/elytra';
const ref = 'master';

const escape = (value) =>
  value
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;');

const urlPath = (value) => value.split(path.sep).join('/');

const md = new MarkdownIt({ html: false, linkify: false, typographer: false });

md.renderer.rules.table_open = () => '<div class="table-scroll"><table>\n';

md.renderer.rules.table_close = () => '</table></div>\n';

md.renderer.rules.heading_open = (tokens, index, options, env, renderer) => {
  const raw = tokens[index + 1].content
    .toLowerCase()
    .replace(/[^\p{L}\p{N}\s-]/gu, '')
    .trim()
    .replace(/\s+/g, '-');
  const count = env.slugs.get(raw) || 0;
  env.slugs.set(raw, count + 1);
  const id = raw + (count ? `-${count}` : '');
  tokens[index].attrSet('id', id);
  if (['h2', 'h3'].includes(tokens[index].tag)) {
    env.headings.push({ id, title: tokens[index + 1].content, level: tokens[index].tag });
  }
  return renderer.renderToken(tokens, index, options);
};

const linkRule =
  md.renderer.rules.link_open ||
  ((tokens, index, options, env, renderer) => renderer.renderToken(tokens, index, options));

md.renderer.rules.link_open = (tokens, index, options, env, renderer) => {
  const token = tokens[index];
  const href = token.attrGet('href');
  if (href && !/^(?:[a-z]+:|#|\/\/)/i.test(href)) {
    const [local, anchor] = href.split('#', 2);
    const target = path.resolve(path.dirname(env.source), decodeURI(local));
    if (target.startsWith(root + path.sep)) {
      if (env.markdown.has(target)) {
        token.attrSet(
          'href',
          urlPath(path.relative(path.dirname(env.source), target)).replace(/\.md$/, '.html') +
            (anchor ? '#' + anchor : '')
        );
      } else {
        token.attrSet(
          'href',
          `${repo}/blob/${ref}/${urlPath(path.relative(root, target))}` +
            (anchor ? '#' + anchor : '')
        );
      }
    }
  }
  return linkRule(tokens, index, options, env, renderer);
};

async function collect(directory) {
  let result = [];
  for (const item of await readdir(directory, { withFileTypes: true })) {
    const name = path.join(directory, item.name);
    if (item.isDirectory()) {
      result.push(...(await collect(name)));
    } else if (name.endsWith('.md')) {
      result.push(name);
    }
  }
  return result;
}

const sources = [path.join(root, 'README.md'), ...(await collect(path.join(root, 'docs')))];
const markdown = new Set(sources);
await mkdir(out, { recursive: true });
for (const item of await readdir(path.join(root, 'site'))) {
  await cp(path.join(root, 'site', item), path.join(out, item), { recursive: true });
}
await writeFile(path.join(out, '.nojekyll'), '');
// Content-addressed assets make a new layout load consistently even when a browser
// still has the previous deployment's CSS/JS in its GitHub Pages cache.
const assets = {};
for (const name of ['styles.css', 'site.mjs', 'mark.svg']) {
  const bytes = await readFile(path.join(root, 'site', name));
  const hash = createHash('sha256').update(bytes).digest('hex').slice(0, 12);
  const parsed = path.parse(name);
  const generated = `${parsed.name}.${hash}${parsed.ext}`;
  assets[name] = generated;
  await writeFile(path.join(out, generated), bytes);
}
let homepage = await readFile(path.join(out, 'index.html'), 'utf8');
for (const [source, target] of Object.entries(assets)) {
  homepage = homepage.replaceAll(`"${source}"`, `"${target}"`);
}
await writeFile(path.join(out, 'index.html'), homepage);

const groups = [
  [
    'Start here',
    [
      ['docs/index', 'Documentation home'],
      ['docs/getting-started', 'Build and run'],
      ['docs/guides/mite-cli', 'Build a CLI'],
      ['docs/guides/http-server', 'HTTP server'],
      ['docs/guides/first-desktop-app', 'First desktop app'],
      ['docs/guides/examples', 'Runtime examples'],
      ['docs/examples/index', 'Desktop examples']
    ]
  ],
  [
    'Concepts',
    [
      ['docs/concepts/runtime-and-processes', 'Runtime and processes'],
      ['docs/concepts/modules-and-packages', 'Modules and packages'],
      ['docs/concepts/async-and-lifetime', 'Async and lifetime']
    ]
  ],
  [
    'API reference',
    [
      ['docs/api/index', 'API overview'],
      ['docs/api/cli', 'Mite CLI'],
      ['docs/api/mite', 'Mite runtime'],
      ['docs/api/crypto', 'node:crypto'],
      ['docs/api/http', 'node:http'],
      ['docs/api/child-process', 'node:child_process'],
      ['docs/api/firefly', 'Firefly overview'],
      ['docs/api/app', 'app'],
      ['docs/api/browser-window', 'BrowserWindow'],
      ['docs/api/web-contents', 'webContents'],
      ['docs/api/context-bridge', 'contextBridge'],
      ['docs/api/ipc', 'ipcMain / ipcRenderer']
    ]
  ],
  [
    'Application guides',
    [
      ['docs/migration/from-node', 'Migrate from Node'],
      ['docs/migration/from-electron', 'Migrate from Electron'],
      ['docs/guides/security', 'Security'],
      ['docs/guides/testing-and-debugging', 'Testing and debugging'],
      ['docs/guides/troubleshooting', 'Troubleshooting'],
      ['docs/guides/packaging', 'Packaging']
    ]
  ],
  [
    'Status and compatibility',
    [
      ['docs/reference/support-policy', 'Support policy'],
      ['docs/compatibility/mite-0.1', 'Mite compatibility'],
      ['docs/compatibility/firefly-0.1', 'Firefly compatibility'],
      ['docs/compatibility/npm-packages-0.1', 'npm packages'],
      ['docs/roadmap', 'Roadmap'],
      ['docs/reference/documentation-baseline', 'Documentation baseline'],
      ['docs/PROGRESS', 'Implementation progress']
    ]
  ],
  [
    'Development',
    [
      ['docs/development/renderer-build', 'Build Firefly'],
      ['docs/development/windows-build', 'Windows bring-up'],
      ['docs/development/linux-build', 'Linux bring-up'],
      ['docs/development/updating-ladybird', 'Update Ladybird'],
      ['docs/development/releases', 'Native releases'],
      ['docs/development/documentation', 'Write documentation'],
      ['docs/development/website', 'Website']
    ]
  ]
];
for (const source of sources) {
  const relative = path.relative(root, source);
  const destination = path.join(out, relative.replace(/\.md$/, '.html'));
  const prefix = urlPath(path.relative(path.dirname(destination), out)) || '.';
  const sourceText = await readFile(source, 'utf8');
  const title = sourceText.match(/^# (.+)$/m)?.[1] || path.basename(source, '.md');
  const env = { source, markdown, slugs: new Map(), headings: [] };
  const content = md.render(sourceText, env);
  const current = urlPath(relative).replace(/\.md$/, '');
  const nav = groups
    .map(
      ([label, links]) =>
        `<details class="doc-group"${links.some(([href]) => href === current) ? ' open' : ''}><summary>${escape(label)}</summary>${links.map(([href, text]) => `<a href="${prefix}/${href}.html"${href === current ? ' aria-current="page"' : ''}>${escape(text)}</a>`).join('')}</details>`
    )
    .join('');
  const outline = env.headings
    .map(
      ({ id, title, level }) => `<a class="toc-${level}" href="#${escape(id)}">${escape(title)}</a>`
    )
    .join('');
  const toc = outline
    ? `<details class="doc-outline" open><summary>On this page</summary><nav aria-label="On this page">${outline}</nav></details>`
    : '';
  const html = `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="theme-color" content="#f8f5eb"><title>${escape(title)} — Mite + Firefly</title><link rel="icon" href="${prefix}/${assets['mark.svg']}" type="image/svg+xml"><link rel="stylesheet" href="${prefix}/${assets['styles.css']}"></head><body class="docs-page"><a class="skip" href="#main">Skip to content</a><header class="nav wrap"><a class="brand" href="${prefix}/"><img src="${prefix}/${assets['mark.svg']}" width="30" height="30" alt="">elytra</a><nav aria-label="Main"><a class="hide-small" href="${prefix}/#projects">Projects</a><a href="${prefix}/docs/index.html">Docs</a><a class="github-link" href="${repo}">GitHub ↗</a></nav></header><div class="wrap docs-layout"><aside class="docs-side"><details class="doc-nav-shell" open><summary>Browse documentation</summary><nav aria-label="Documentation"><a class="doc-home" href="${prefix}/docs/index.html">DOCUMENTATION</a>${nav}</nav></details></aside><main class="doc-article" id="main">${toc}${content}<a class="doc-source" href="${repo}/blob/${ref}/${urlPath(relative)}">View this page on GitHub ↗</a></main></div><footer class="wrap footer"><a class="brand" href="${prefix}/">elytra<span>/</span></a><p>Mite + Firefly · Open source, under development.</p><a href="${repo}">Contribute ↗</a></footer><script type="module" src="${prefix}/${assets['site.mjs']}"></script></body></html>`;
  await mkdir(path.dirname(destination), { recursive: true });
  await writeFile(destination, html);
}
console.log(
  `Built homepage and ${sources.length} documentation pages in ${path.relative(root, out)}`
);
