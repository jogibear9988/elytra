/** Read only the public web tree for the desktop protocol and standalone HTTP server. */
import { createServer } from 'node:http';
import { readFile, realpath, stat } from 'node:fs/promises';
import { extname, relative, resolve, sep, isAbsolute } from 'node:path';

const mime = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json',
  '.css': 'text/css; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.gif': 'image/gif',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.ttf': 'font/ttf',
  '.wasm': 'application/wasm'
};

export async function createAssetReader(directory) {
  const root = await realpath(resolve(directory));
  return async (pathname, method, smoke = false) => {
    if (method !== 'GET' && method !== 'HEAD') {
      return { status: 405, headers: { Allow: 'GET, HEAD' } };
    }

    try {
      const path = decodeURIComponent(pathname);
      if (
        !path.startsWith('/') ||
        /[\x00-\x1f\\:"<>|?*]/u.test(path) ||
        path.split('/').some((part) => part.startsWith('.') || part.endsWith(' '))
      ) {
        return { status: 400 };
      }

      if (path === '/__firefly_smoke.js' && smoke) {
        const bytes = await readFile(new URL('./smoke.mjs', import.meta.url));
        return publicAsset(bytes, '.mjs', method);
      }

      const file = await realpath(resolve(root, '.' + (path === '/' ? '/index.html' : path)));
      const local = relative(root, file);
      if (local === '..' || local.startsWith('..' + sep) || isAbsolute(local)) {
        return { status: 403 };
      }
      if (!(await stat(file)).isFile()) {
        return { status: 404 };
      }
      let bytes = await readFile(file);
      if (smoke && local === 'index.html') {
        bytes = Buffer.from(
          bytes
            .toString()
            .replace('</head>', '<script type="module" src="/__firefly_smoke.js"></script></head>')
        );
      }
      return publicAsset(bytes, extname(file).toLowerCase(), method);
    } catch (error) {
      if (error instanceof URIError) {
        return { status: 400 };
      }
      return { status: error.code === 'ENOENT' || error.code === 'ENOTDIR' ? 404 : 500 };
    }
  };
}

function publicAsset(bytes, extension, method) {
  return {
    status: 200,
    headers: {
      'Content-Type': mime[extension] || 'application/octet-stream',
      'Content-Length': bytes.length,
      'X-Content-Type-Options': 'nosniff',
      'Cache-Control': 'no-store'
    },
    body: method === 'HEAD' ? undefined : bytes
  };
}

export async function startServer(directory) {
  const readAsset = await createAssetReader(directory);
  const server = createServer(async (request, response) => {
    let url;
    try {
      url = new URL(request.url, 'http://127.0.0.1');
    } catch {
      response.writeHead(400);
      response.end();
      return;
    }
    const { status, headers, body } = await readAsset(url.pathname, request.method);
    response.writeHead(status, headers || {});
    response.end(body);
  });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  return server;
}
