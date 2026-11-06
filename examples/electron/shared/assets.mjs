import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

// Static public assets only. No directory traversal, native action route, runtime
// config, main/utility source or preload bundle is exposed over HTTP.
export async function serveAssets(source, id) {
  const allowed = [
    'index.html',
    'second.html',
    'renderer.mjs',
    'worker.mjs',
    'embedded.html',
    'embedded.mjs'
  ];
  const routes = new Map(allowed.map((name) => [`/${id}/${name}`, new URL('./' + name, source)]));
  routes.set('/shared/style.css', new URL('../shared/style.css', source));
  let port;
  const server = createServer(async (request, response) => {
    try {
      if (!['GET', 'HEAD'].includes(request.method)) {
        response.statusCode = 405;
        response.end();
        return;
      }
      if (request.headers.host !== `127.0.0.1:${port}`) {
        response.statusCode = 400;
        response.end();
        return;
      }
      const url = new URL(request.url, `http://127.0.0.1:${port}`);
      const file = routes.get(url.pathname);
      if (!file || url.search) {
        response.statusCode = 404;
        response.end();
        return;
      }
      const bytes = await readFile(fileURLToPath(file));
      const type = file.pathname.endsWith('.css')
        ? 'text/css'
        : file.pathname.endsWith('.mjs')
          ? 'text/javascript'
          : 'text/html';
      response.setHeader('Content-Type', type + '; charset=utf-8');
      response.setHeader('Content-Length', bytes.length);
      response.setHeader('Connection', 'close');
      response.setHeader('Cache-Control', 'no-store');
      response.end(request.method === 'HEAD' ? undefined : bytes);
    } catch (error) {
      response.statusCode = error.code === 'ENOENT' ? 404 : 500;
      response.end();
    }
  });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  port = server.address().port;
  return { server, url: `http://127.0.0.1:${port}/${id}/index.html` };
}
