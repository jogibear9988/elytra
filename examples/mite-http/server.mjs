import { createServer } from 'node:http';

const port = Number(process.argv[2] ?? 3000);
const server = createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/echo') {
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' });

    req.on('error', (error) => res.destroy(error));

    req.pipe(res);
    return;
  }
  res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
  res.end(JSON.stringify({ runtime: 'Mite', method: req.method, url: req.url }) + '\n');
});

server.on('error', (error) => {
  console.error(error.code);
  process.exitCode = 1;
});

server.listen(port, '127.0.0.1', () => {
  console.log(`HTTP listening on http://127.0.0.1:${server.address().port}`);
});
