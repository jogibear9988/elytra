/** Cancel an incomplete HTTP response while handling both request and body errors. */
import { createServer, get } from 'node:http';

const controller = new AbortController();
const server = createServer((request, response) => {
  response.writeHead(200, { 'content-length': 100 });
  response.write('partial');
});

server.on('error', (error) => {
  throw error;
});

server.listen(0, '127.0.0.1', () => {
  const request = get(
    {
      host: '127.0.0.1',
      port: server.address().port,
      agent: false,
      signal: AbortSignal.any([controller.signal, AbortSignal.timeout(5000)])
    },
    (response) => {
      response.on('error', (error) => {
        if (error.code !== 'ECONNRESET') {
          throw error;
        }
      });

      response.once('data', (data) => {
        console.log('Received:', data.toString());
        controller.abort('Response no longer needed');
      });

      response.once('close', () => {
        console.log('Response incomplete:', !response.complete);

        server.close(() => console.log('Server closed'));
      });
    }
  );

  request.on('error', (error) => {
    if (error.code !== 'ABORT_ERR' || error.cause !== 'Response no longer needed') {
      throw error;
    }
    console.log('Request cancelled:', error.cause);
  });
});
