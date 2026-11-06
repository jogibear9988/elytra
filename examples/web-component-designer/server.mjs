/**
 * Serve public assets and scope sample-only native/smoke routes with validated tokens.
 */
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { resolve, sep, extname } from 'node:path';

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
// Serve only the compiled, public web tree, never the app's main-process sources.
// Loopback HTTP preserves module/worker origin semantics without file-URL rewrites.
export async function startServer(directory, smoke, controls) {
  const root = resolve(directory);
  const server = createServer(async (request, response) => {
    try {
      const target = new URL(request.url, 'http://127.0.0.1');
      if (target.pathname.startsWith('/__firefly_window/')) {
        const origin = `http://127.0.0.1:${server.address().port}`;
        const action = target.pathname.slice('/__firefly_window/'.length);
        if (!controls) {
          response.statusCode = 404;
          response.end();
          return;
        }
        if (
          request.headers['transfer-encoding'] ||
          (request.headers['content-length'] && request.headers['content-length'] !== '0') ||
          request.method !== 'POST' ||
          request.headers.origin !== origin ||
          request.headers.host !== origin.slice(7) ||
          request.headers['x-firefly-window-token'] !== controls.token
        ) {
          response.statusCode = 403;
          response.end();
          return;
        }
        if (!['minimize', 'maximize', 'unmaximize', 'restore', 'state', 'close'].includes(action)) {
          response.statusCode = 400;
          response.end();
          return;
        }
        response.setHeader('Content-Type', 'application/json');
        response.setHeader('Cache-Control', 'no-store');
        response.end(JSON.stringify(controls.execute(action)));
        return;
      }
      if (smoke && target.searchParams.get('token') === smoke.token) {
        if (target.pathname === '/__firefly_smoke_stage' && request.method === 'GET') {
          const stage = target.searchParams.get('stage');
          if (
            [
              'defined',
              'container',
              'ready',
              'edited',
              'monaco',
              'geometry',
              'controls',
              'close'
            ].includes(stage)
          ) {
            console.log('DESIGNER_STAGE ' + stage);
          }
          response.statusCode = 204;
          response.end();
          return;
        }
        if (target.pathname === '/__firefly_smoke.js' && request.method === 'GET') {
          response.setHeader('Content-Type', 'text/javascript');
          response.end(smoke.script);
          return;
        }
        if (target.pathname === '/__firefly_smoke_result' && request.method === 'POST') {
          let bytes = 0;
          let body = '';

          request.on('data', (chunk) => {
            bytes += chunk.length;
            if (bytes <= 1024) {
              body += chunk.toString();
            }
          });

          request.on('end', () => {
            response.statusCode = bytes > 1024 ? 413 : 204;
            response.end();
            if (bytes <= 1024) {
              smoke.report(body);
            }
          });

          return;
        }
      }
      if (request.method !== 'GET' && request.method !== 'HEAD') {
        response.statusCode = 405;
        response.setHeader('Allow', 'GET, HEAD');
        response.end();
        return;
      }
      const pathname = decodeURIComponent(new URL(request.url, 'http://127.0.0.1').pathname);
      if (
        pathname.includes('\0') ||
        pathname.includes('\\') ||
        pathname.split('/').some((part) => part === '..' || part.startsWith('.'))
      ) {
        response.statusCode = 400;
        response.end();
        return;
      }
      const file = resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
      if (!file.startsWith(root + sep)) {
        response.statusCode = 403;
        response.end();
        return;
      }
      let bytes = await readFile(file);
      if (controls && file === resolve(root, 'index.html')) {
        bytes = Buffer.from(
          bytes
            .toString()
            .replace(
              '</head>',
              `<meta name="firefly-window-token" content="${controls.token}"></head>`
            )
        );
      }
      if (smoke && file === resolve(root, 'index.html')) {
        bytes = Buffer.from(
          bytes
            .toString()
            .replace(
              '</head>',
              `<script type="module" src="/__firefly_smoke.js?token=${smoke.token}" defer></script></head>`
            )
        );
      }
      response.setHeader(
        'Content-Type',
        mime[extname(file).toLowerCase()] || 'application/octet-stream'
      );
      response.setHeader('Content-Length', bytes.length);
      response.setHeader('X-Content-Type-Options', 'nosniff');
      response.setHeader('Cache-Control', 'no-store');
      response.end(request.method === 'HEAD' ? undefined : bytes);
    } catch (error) {
      if (smoke && error.code === 'ENOENT') {
        console.error('DESIGNER_ASSET_MISSING ' + request.url);
      }
      response.statusCode =
        error.code === 'ENOENT' || error.code === 'EISDIR'
          ? 404
          : error instanceof URIError
            ? 400
            : 500;
      response.end();
    }
  });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  return server;
}
