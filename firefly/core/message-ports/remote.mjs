/** Private native utility links. Public wrappers never receive route identities or transport hooks. */
export default function createRemoteLinks(hooks) {
  const links = new Set();
  const shift = Array.prototype.shift;
  const apply = Reflect.apply;

  function descriptor(value) {
    return (
      value &&
      Number.isInteger(value.id) &&
      value.id > 0 &&
      value.id <= 0x7fffffff &&
      Number.isInteger(value.generation) &&
      value.generation > 0 &&
      value.generation <= 0x7fffffff
    );
  }

  function connect(transport) {
    const routes = new Map();
    let disconnected = false;
    let flushing = false;

    function valid(values) {
      if (disconnected || !Array.isArray(values) || values.length > 16) {
        throw hooks.error('ERR_FIREFLY_PORT_TRANSFER', 'Invalid native port attachment');
      }
      const ids = new Set();
      for (const value of values) {
        if (!descriptor(value) || ids.has(value.id) || routes.has(value.id)) {
          throw hooks.error('ERR_FIREFLY_PORT_TRANSFER', 'Duplicate or invalid native port route');
        }
        ids.add(value.id);
      }
    }

    function disconnect() {
      if (disconnected) {
        return;
      }
      disconnected = true;
      links.delete(link);
      const owned = [...routes.values()];
      routes.clear();
      for (const route of owned) {
        route.closed = true;
        route.acks.length = 0;
        hooks.close(route.record, false);
      }
    }

    function accepted(status) {
      if (!status) {
        return true;
      }
      if (transport.blocked(status)) {
        return false;
      }
      disconnect();
      return false; // A failed transport retires its ports and delivers peer close.
    }

    /** Failed control writes remain pending; a writable notification retries without redelivery. */
    function flush() {
      if (flushing || disconnected) {
        return;
      }
      flushing = true;
      try {
        for (const route of routes.values()) {
          if (route.closing) {
            if (accepted(transport.close(route.id, route.generation))) {
              route.closed = true;
              route.acks.length = 0;
              routes.delete(route.id);
            }
            continue;
          }
          while (route.acks.length && !disconnected) {
            if (!accepted(transport.consume(route.id, route.generation, route.acks[0]))) {
              break;
            }
            apply(shift, route.acks, []);
          }
        }
      } finally {
        flushing = false;
      }
    }

    function bind(record, value) {
      const route = {
        id: value.id,
        generation: value.generation,
        record,
        acks: [],
        closing: false,
        closed: false,

        send(wire) {
          return (
            !disconnected &&
            !route.closing &&
            accepted(transport.send(route.id, route.generation, wire))
          );
        },

        requestClose() {
          if (!disconnected && !route.closed) {
            route.closing = true;
            route.acks.length = 0;
            flush();
          }
        }
      };
      record.remote = route;
      record.started = true;
      routes.set(route.id, route);
      return route;
    }

    const link = {
      /** Prepare entirely before native admission; failed writes leave every source owner attached. */
      prepare(transfer) {
        if (disconnected) {
          throw hooks.error('ERR_FIREFLY_PORT_CHANNEL', 'Native port link is closed');
        }
        const moves = hooks.transfers(transfer, null, null);
        for (const move of moves) {
          if (move.record.peer?.remote) {
            throw hooks.error(
              'ERR_FIREFLY_UNSUPPORTED_API',
              'Forwarding a native-backed port is not implemented'
            );
          }
          if (move.record.queue.some((entry) => entry.ports?.length)) {
            throw hooks.error(
              'ERR_FIREFLY_UNSUPPORTED_API',
              'Queued native port attachments are not implemented'
            );
          }
        }
        // A later transfer-list descriptor may have moved/closed an earlier
        // entry. Reject that before admitting any native attachment frame.
        function unchanged() {
          for (const move of moves) {
            if (
              move.owned.record !== move.record ||
              move.owned.detached ||
              move.record.closed ||
              move.record.lease.generation !== move.generation
            ) {
              throw hooks.error(
                'ERR_FIREFLY_PORT_TRANSFER',
                'Port ownership changed while preparing native transfer'
              );
            }
          }
        }

        unchanged();
        let committed = false;
        return {
          count: moves.length,

          commit(values) {
            valid(values);
            if (committed || values.length !== moves.length) {
              disconnect();
              throw hooks.error(
                'ERR_FIREFLY_PORT_TRANSFER',
                'Native port attachment count changed'
              );
            }
            unchanged();
            committed = true;
            for (let index = 0; index < moves.length; index++) {
              const move = moves[index];
              hooks.detach(move);
              bind(move.record, values[index]);
              hooks.pump(move.record);
            }
          }
        };
      },

      /** Incoming attachments become genuine native-backed utility endpoints before parent-event delivery. */
      adopt(values) {
        valid(values);
        hooks.capacity(values.length * 2);
        const owners = values.map(() => hooks.shell());
        // Emitter constructors can reenter application code. Finish all shells
        // before rechecking channel lifetime and the global endpoint quota.
        valid(values);
        hooks.capacity(values.length * 2);
        const ports = [];
        const allocated = [];
        try {
          for (let index = 0; index < values.length; index++) {
            const local = hooks.endpoint();
            allocated.push(local);
            const remote = hooks.endpoint();
            allocated.push(remote);
            local.peer = remote;
            remote.peer = local;
            bind(remote, values[index]);
            ports.push(hooks.attach(local, owners[index]));
          }
        } catch (failure) {
          for (const record of allocated) {
            hooks.close(record, false);
          }
          disconnect();
          throw failure;
        }
        return ports;
      },

      receive(id, generation, wire) {
        const route = routes.get(id);
        if (
          !route ||
          route.generation !== generation ||
          route.closed ||
          route.closing ||
          disconnected
        ) {
          return;
        }
        const size = hooks.size(wire);

        const ack = () => {
          if (!route.closed && !route.closing && !disconnected) {
            route.acks.push(size);
            flush();
          }
        };

        if (!hooks.deliver(route.record.peer, wire, ack)) {
          route.requestClose();
        }
      },

      closed(id, generation) {
        const route = routes.get(id);
        if (!route || route.generation !== generation) {
          return;
        }
        route.closed = true;
        route.acks.length = 0;
        routes.delete(id);
        hooks.close(route.record, false);
      },

      writable() {
        flush();
        if (!disconnected) {
          for (const route of routes.values()) {
            hooks.pump(route.record);
          }
        }
      },

      disconnect
    };
    links.add(link);
    return link;
  }

  function shutdown() {
    for (const link of [...links]) {
      link.disconnect();
    }
  }

  return { connect, shutdown };
}
