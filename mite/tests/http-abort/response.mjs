/** Abort an incomplete response and preserve Node's request/response event order. */
import { createServer, get } from 'node:http';
const server = createServer((request, response) => {
  response.writeHead(200, { 'content-length': 100 });
  response.write('partial');
});

server.listen(0, '127.0.0.1', () => {
  const controller = new AbortController();
  const reason = { response: true };
  const events = [];
  let correct = false;
  const request = get(
    { host: '127.0.0.1', port: server.address().port, agent: false, signal: controller.signal },
    (response) => {
      events.push('response');

      response.on('aborted', () => events.push('aborted'));

      response.on('error', (error) => events.push('response:' + error.code));

      response.on('end', () => events.push('end'));

      response.once('close', () => {
        events.push('response:close');

        server.close(() =>
          console.log(
            'response abort',
            events.join(','),
            correct,
            request.aborted,
            response.complete
          )
        );
      });

      response.once('data', () => {
        events.push('data');
        controller.abort(reason);
      });
    }
  );

  request.on('error', (error) => {
    correct = error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === reason;
    events.push('request:error');
  });

  request.on('close', () => events.push('request:close'));
});
