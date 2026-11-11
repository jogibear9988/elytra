/** Document-scoped native links; application code receives ports, never native route credentials. */
import decodePacket from './packets.mjs';

export default function createRendererPorts(native, channels, error) {
  const links = new Map();
  const apply = Reflect.apply;
  const byteLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    'byteLength'
  ).get;

  function disconnect(id) {
    const current = links.get(id);
    if (current) {
      links.delete(id);
      current.link.disconnect();
    }
  }

  function current(id, epoch) {
    const old = links.get(id);
    if (old?.epoch === epoch) {
      return old;
    }
    disconnect(id);
    const link = channels.connect({
      send: (route, generation, wire) =>
        native.sendRendererPort(id, epoch, route, generation, wire),

      close: (route, generation) => native.closeRendererPort(id, epoch, route, generation),

      consume: (route, generation, size) =>
        native.consumeRendererPort(id, epoch, route, generation, size),

      blocked: (status) => status === native.rendererPortQueueFull,
      // The other peer can already belong to a utility: this is the actual
      // SharedProcess relay. Utility-to-utility forwarding remains refused.
      allowNativeRelay: true,
      maxPayload: 128 * 1024 - 20 - 16
    });
    const record = { epoch, link };
    links.set(id, record);
    return record;
  }

  function post(id, channel, message, transfer) {
    if (typeof channel !== 'string' || channel.length > 256) {
      throw error('ERR_INVALID_ARG_TYPE', 'postMessage requires a bounded channel string');
    }
    const wire = native.encodeIpc([channel, message]);
    if (transfer === undefined || (Array.isArray(transfer) && transfer.length === 0)) {
      native.sendIpc(id, wire);
      return;
    }
    if (typeof native.portEpoch !== 'function' || typeof native.sendPortTransfer !== 'function') {
      throw error(
        'ERR_FIREFLY_UNSUPPORTED_API',
        'Native renderer port delivery is not implemented'
      );
    }
    const epoch = native.portEpoch(id);
    if (typeof epoch !== 'string' || !epoch.length) {
      throw error('ERR_FIREFLY_IPC_NOT_READY', 'No active document for port delivery');
    }
    const owner = current(id, epoch);
    const plan = owner.link.prepare(transfer);
    if (apply(byteLength, wire, []) > 128 * 1024 - 20 - 16 - plan.count * 8) {
      throw error('ERR_FIREFLY_PORT_LIMIT', 'Port attachment message exceeds its native frame');
    }
    const result = native.sendPortTransfer(id, epoch, wire, plan.count);
    if (!result || result.status !== 0 || result.epoch !== epoch) {
      throw error('ERR_FIREFLY_PORT_CHANNEL', 'Native document port admission failed');
    }
    try {
      plan.commit(result.ports);
    } catch (failure) {
      disconnect(id);
      throw failure;
    }
  }

  function dispatch(id, type, data) {
    const owner = links.get(id);
    // Native emissions carry their original document epoch; delayed callbacks
    // from a retired document cannot address a newer link in the same window.
    if (!owner || !data || data.epoch !== owner.epoch) {
      return;
    }
    if (type === 'renderer-port-revoked') {
      disconnect(id);
    } else if (type === 'renderer-port-writable') {
      owner.link.writable();
    } else if (type === 'renderer-port-data' || type === 'renderer-port-close') {
      try {
        const kind = type === 'renderer-port-data' ? 'port-data' : 'port-close';
        const packet = decodePacket(data.wire, kind);
        if (kind === 'port-data') {
          owner.link.receive(packet.id, packet.generation, packet.wire);
        } else {
          owner.link.closed(packet.id, packet.generation);
        }
      } catch (failure) {
        disconnect(id);
        throw failure;
      }
    }
  }

  function shutdown() {
    for (const id of [...links.keys()]) {
      disconnect(id);
    }
  }

  return { post, dispatch, disconnect, shutdown };
}
