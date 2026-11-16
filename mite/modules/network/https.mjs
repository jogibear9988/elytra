/** Share authoritative HTTP classes/framing while requiring verified native TLS transport. */
import { getAgentName } from './http/agent.mjs';

export default function createHttpsModule(host) {
  function unsupported(name) {
    throw Object.assign(new Error(`${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  }

  function tlsOptions(config, request) {
    // HTTP owns path/framing/signal/timeout. Forward TLS policy fields so an
    // unsupported security option is rejected rather than silently dropped.
    const options = { ...config.opts };
    for (const key of [
      'path',
      'method',
      'headers',
      'auth',
      'agent',
      'protocol',
      'hostname',
      'port',
      'signal',
      'timeout'
    ]) {
      delete options[key];
    }
    let name = String(request?.getHeader('host') ?? config.hostname);
    name = name.startsWith('[') ? name.slice(1, name.indexOf(']')) : name.split(':')[0];
    options.host = config.hostname;
    options.servername = config.opts.servername ?? (host.net.isIP(name) ? '' : name);
    options.ALPNProtocols ??= ['http/1.1'];
    if (
      !Array.isArray(options.ALPNProtocols) ||
      options.ALPNProtocols.some((value) => value !== 'http/1.1')
    ) {
      unsupported('HTTPS ALPN other than http/1.1');
    }
    return options;
  }

  const transport = {
    protocol: 'https:',
    defaultPort: 443,
    readyEvent: 'secureConnect',

    isSocket(socket) {
      return host.tlsHooks.isSocket(socket);
    },

    agentOptions(config, request) {
      const options = tlsOptions(config, request);
      host.tlsHooks.validateOptions(options);
      return {
        ...config.opts,
        servername: options.servername,
        ALPNProtocols: options.ALPNProtocols
      };
    },

    createSocket(config, request) {
      return host.tlsHooks.createSocket(tlsOptions(config, request));
    }
  };

  /** HTTPS admission shares real HTTP Agent lifecycle and uses verified TLS. */
  class Agent extends host.httpHooks.Agent {
    constructor(options = {}) {
      if (options?.maxCachedSessions !== undefined && options.maxCachedSessions !== 0) {
        unsupported('HTTPS Agent TLS session caching');
      }
      super(options);
      this.protocol = 'https:';
      this.defaultPort = 443;
      this.maxCachedSessions = 0;
    }

    getName(options = {}) {
      // agent-base deliberately calls this with its own HTTP Agent receiver.
      return getAgentName(options, true);
    }

    createConnection(options, callback) {
      const config = { opts: options, hostname: options.host ?? options.hostname ?? 'localhost' };
      const socket = transport.createSocket(config);
      if (callback) {
        socket.once('secureConnect', () => callback(null, socket));
      }
      try {
        socket.connect({
          port: options.port ?? 443,
          host: config.hostname,
          family: options.family,
          lookup: options.lookup,
          hints: options.hints,
          autoSelectFamily: options.autoSelectFamily,
          autoSelectFamilyAttemptTimeout: options.autoSelectFamilyAttemptTimeout
        });
      } catch (error) {
        socket.destroy();
        throw error;
      }
      return socket;
    }
  }

  const globalAgent = new Agent();
  transport.globalAgent = globalAgent;
  Object.freeze(transport);

  function request(input, options, callback) {
    return host.httpHooks.request(transport, input, options, callback);
  }

  function get(input, options, callback) {
    const result = request(input, options, callback);
    result.end();
    return result;
  }

  class Server {
    constructor() {
      unsupported('HTTPS servers');
    }
  }

  return {
    request,
    get,
    Agent,
    globalAgent,
    Server,

    createServer() {
      unsupported('HTTPS servers');
    }
  };
}
