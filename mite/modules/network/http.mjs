/**
 * Assemble one HTTP builtin per runtime/realm from explicit protocol, message,
 * client and server boundaries. Native dependencies come from the embedding
 * loader; the private HTTPS transport hook never becomes a public export.
 */
import createHttpProtocol from './http/protocol.mjs';
import createHttpMessages from './http/messages.mjs';
import createHttpClient from './http/client.mjs';
import createHttpServer from './http/server.mjs';

export default function createHttpModule(host) {
  const protocol = createHttpProtocol(host.native);
  const messages = createHttpMessages(host, protocol);
  const client = createHttpClient(host, protocol, messages);
  const server = createHttpServer(host, protocol, messages);
  Object.defineProperty(host.clientHooks, 'request', { value: client.requestWithTransport });

  return {
    Server: server.Server,
    ServerResponse: messages.ServerResponse,
    OutgoingMessage: messages.OutgoingMessage,
    IncomingMessage: messages.IncomingMessage,
    ClientRequest: client.ClientRequest,
    request: client.request,
    get: client.get,
    createServer: server.createServer,
    METHODS: protocol.METHODS,
    STATUS_CODES: protocol.STATUS_CODES,
    validateHeaderName: protocol.validateHeaderName,
    validateHeaderValue: protocol.validateHeaderValue,
    maxHeaderSize: 16384
  };
}
