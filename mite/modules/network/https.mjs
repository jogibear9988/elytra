/** Share authoritative HTTP classes/framing while requiring verified native TLS transport. */
export default function createHttpsModule(host) {
  function unsupported(name) {
    throw Object.assign(new Error(`${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  }

  const transport = Object.freeze({
    protocol: 'https:',
    defaultPort: 443,
    readyEvent: 'secureConnect',

    createSocket(config, request) {
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
      let name = String(request.getHeader('host') ?? config.hostname);
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
      return host.tlsHooks.createSocket(options);
    }
  });

  function request(input, options, callback) {
    return host.httpHooks.request(transport, input, options, callback);
  }

  function get(input, options, callback) {
    const result = request(input, options, callback);
    result.end();
    return result;
  }

  class Agent {
    constructor() {
      unsupported('HTTPS Agent/pooling');
    }
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
    Server,

    createServer() {
      unsupported('HTTPS servers');
    }
  };
}
