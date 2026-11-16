/**
 * Assemble one HTTP builtin per runtime/realm from explicit protocol, message,
 * client and server boundaries. Native dependencies come from the embedding
 * loader; the private HTTPS transport hook never becomes a public export.
 */
import createHttpProtocol from './http/protocol.mjs';
import createHttpMessages from './http/messages.mjs';
import createHttpClient from './http/client.mjs';
import createHttpServer from './http/server.mjs';
import createHttpAgent from './http/agent.mjs';

export default function createHttpModule(host) {
  const protocol = createHttpProtocol(host.native);
  const messages = createHttpMessages(host, protocol);
  const agents = createHttpAgent(host, 'http:', (options) => {
    for (const key of ['socketPath', 'localAddress', 'localPort', 'proxyEnv']) {
      if (options[key] !== undefined) {
        protocol.unsupported(`HTTP connection option ${key}`);
      }
    }
    if (options.path != null) {
      protocol.unsupported('HTTP Unix-domain connections');
    }
    // HTTP's request path is not a Unix-domain socket path. Forward only
    // connection fields to the genuine TCP connector.
    return host.net.createConnection({
      port: options.port ?? 80,
      host: options.host ?? options.hostname ?? 'localhost',
      family: options.family,
      lookup: options.lookup,
      hints: options.hints,
      autoSelectFamily: options.autoSelectFamily,
      autoSelectFamilyAttemptTimeout: options.autoSelectFamilyAttemptTimeout,
      noDelay: options.noDelay ?? true,
      keepAlive: false,
      highWaterMark: options.highWaterMark
    });
  });
  const client = createHttpClient(host, protocol, messages, agents);
  const server = createHttpServer(host, protocol, messages);
  Object.defineProperty(host.clientHooks, 'request', { value: client.requestWithTransport });
  Object.defineProperty(host.clientHooks, 'Agent', { value: agents.Agent });

  return {
    Agent: agents.Agent,
    globalAgent: agents.globalAgent,
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
