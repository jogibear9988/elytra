/**
 * Assemble one realm-local TCP/local-pipe builtin from private validation, connection,
 * socket-stream and listener policies. Public ESM/CJS aliases share the same
 * constructors and functions; native I/O remains in the embedding adapter.
 */
import createNetworkPolicy from './net/policy.mjs';
import createSockets from './net/socket.mjs';
import createServers from './net/server.mjs';

export default function createNetworkModule(host) {
  const policy = createNetworkPolicy(host);
  const { Socket, accepted, connect } = createSockets(host, policy);
  const { Server, createServer } = createServers(host, policy, accepted);
  const { ip } = policy;

  return {
    Socket,
    Server,
    createServer,
    connect,
    createConnection: connect,
    isIP: ip,

    isIPv4: (value) => ip(value) === 4,

    isIPv6: (value) => ip(value) === 6
  };
}
