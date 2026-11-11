/** Bounded independent-heap message snapshots. Never move JavaScript objects between VMs. */
export default function createClone(types) {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder('utf-8', { fatal: true });
  const arrayBufferBytes = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get;
  const typedPrototype = Object.getPrototypeOf(Uint8Array.prototype);
  const typedBuffer = Object.getOwnPropertyDescriptor(typedPrototype, 'buffer').get;
  const typedOffset = Object.getOwnPropertyDescriptor(typedPrototype, 'byteOffset').get;
  const typedBytes = Object.getOwnPropertyDescriptor(typedPrototype, 'byteLength').get;
  const keys = Object.keys;
  const proto = Object.getPrototypeOf;
  const define = Object.defineProperty;

  function fail(message) {
    throw new DOMException(message, 'DataCloneError');
  }

  function encode(value) {
    const seen = new Map();
    const nodes = [];
    let properties = 0;
    let characters = 0;
    let binaryBytes = 0;

    function string(value) {
      characters += value.length;
      if (characters > 4 * 1024 * 1024) {
        fail('Worker message string budget exceeded');
      }
      return value;
    }

    function visit(value, depth) {
      if (depth > 128) {
        fail('Worker message nesting budget exceeded');
      }
      if (value === null || typeof value === 'boolean') {
        return value;
      }
      if (typeof value === 'string') {
        return string(value);
      }
      if (typeof value === 'number') {
        if (Object.is(value, -0)) {
          return ['n', '-0'];
        }
        return Number.isFinite(value) ? value : ['n', String(value)];
      }
      if (typeof value === 'undefined') {
        return ['u'];
      }
      if (typeof value === 'bigint') {
        return ['i', string(String(value))];
      }
      if (typeof value !== 'object' || types.isProxy(value)) {
        fail('Value cannot be cloned');
      }
      if (types.isSharedArrayBuffer(value)) {
        fail('Shared memory is not supported by workers');
      }
      if (seen.has(value)) {
        return ['r', seen.get(value)];
      }
      if (nodes.length >= 65536) {
        fail('Worker message node budget exceeded');
      }
      const id = nodes.length;
      seen.set(value, id);
      nodes.push(null);
      if (types.isArrayBuffer(value) || types.isUint8Array(value)) {
        if (types.isUint8Array(value)) {
          const backing = typedBuffer.call(value);
          if (types.isSharedArrayBuffer(backing)) {
            fail('Shared worker buffers are unsupported');
          }
          const offset = typedOffset.call(value);
          const length = typedBytes.call(value);
          try {
            new Uint8Array(backing, offset, length);
          } catch {
            fail('Detached worker buffer');
          }
          nodes[id] = ['v', visit(backing, depth + 1), offset, length];
        } else {
          let bytes;
          try {
            bytes = new Uint8Array(value, 0, arrayBufferBytes.call(value));
          } catch {
            fail('Detached worker message buffer');
          }
          binaryBytes += bytes.byteLength;
          if (binaryBytes > 1024 * 1024) {
            fail('Worker binary snapshot exceeds 1 MiB');
          }
          nodes[id] = ['b', Array.from(bytes)];
        }
      } else {
        const array = Array.isArray(value);
        if (!array && proto(value) !== Object.prototype && proto(value) !== null) {
          fail('This worker clone supports plain objects, arrays and byte buffers');
        }
        const names = keys(value);
        properties += names.length;
        if (properties > 131072 || (array && value.length > 65536)) {
          fail('Worker message property budget exceeded');
        }
        const entries = names.map((name) => [string(name), visit(value[name], depth + 1)]);
        nodes[id] = [array ? 'a' : 'o', entries, array ? value.length : 0];
      }
      return ['r', id];
    }

    const root = visit(value, 0);
    const bytes = encoder.encode(JSON.stringify([1, root, nodes]));
    if (bytes.byteLength > 8 * 1024 * 1024) {
      fail('Worker message exceeds 8 MiB');
    }
    return bytes;
  }

  function decode(bytes) {
    if (bytes.byteLength > 8 * 1024 * 1024) {
      fail('Worker wire exceeds 8 MiB');
    }
    const [version, root, nodes] = JSON.parse(decoder.decode(bytes));
    if (version !== 1 || !Array.isArray(nodes) || nodes.length > 65536) {
      fail('Invalid worker wire');
    }
    let binaryBytes = 0;
    const objects = nodes.map((node) => {
      if (!Array.isArray(node)) {
        fail('Invalid worker node');
      }
      if (node[0] === 'a') {
        if (!Number.isInteger(node[2]) || node[2] < 0 || node[2] > 65536) {
          fail('Invalid worker array');
        }
        return new Array(node[2]);
      }
      if (node[0] === 'o') {
        return {};
      }
      if (node[0] === 'v') {
        return null;
      }
      if (node[0] === 'b') {
        if (
          !Array.isArray(node[1]) ||
          node[1].length > 1024 * 1024 ||
          node[1].some((n) => !Number.isInteger(n) || n < 0 || n > 255)
        ) {
          fail('Invalid worker bytes');
        }
        binaryBytes += node[1].length;
        if (binaryBytes > 1024 * 1024) {
          fail('Worker binary snapshot exceeds 1 MiB');
        }
        const result = new Uint8Array(node[1]);
        return result.buffer;
      }
      fail('Invalid worker type');
    });

    function unpack(value) {
      if (!Array.isArray(value)) {
        if (value === null || ['boolean', 'number', 'string'].includes(typeof value)) {
          return value;
        }
        fail('Invalid worker scalar');
      }
      if (
        value[0] === 'r' &&
        value.length === 2 &&
        Number.isInteger(value[1]) &&
        value[1] >= 0 &&
        value[1] < objects.length
      ) {
        return objects[value[1]];
      }
      if (value[0] === 'u' && value.length === 1) {
        return undefined;
      }
      if (value[0] === 'i' && value.length === 2 && typeof value[1] === 'string') {
        return BigInt(value[1]);
      }
      if (value[0] === 'n' && ['-0', 'NaN', 'Infinity', '-Infinity'].includes(value[1])) {
        return Number(value[1]);
      }
      fail('Invalid worker reference');
    }

    // Backing buffers are allocated before views, preserving offset and shared
    // buffer identity across multiple private views in the same snapshot.
    for (let id = 0; id < nodes.length; id++) {
      const node = nodes[id];
      if (node[0] !== 'v') {
        continue;
      }
      const backing = unpack(node[1]);
      if (
        !types.isArrayBuffer(backing) ||
        !Number.isInteger(node[2]) ||
        !Number.isInteger(node[3]) ||
        node[2] < 0 ||
        node[3] < 0
      ) {
        fail('Invalid worker view');
      }
      try {
        objects[id] = new Uint8Array(backing, node[2], node[3]);
      } catch {
        fail('Invalid worker view bounds');
      }
    }
    let properties = 0;
    for (let id = 0; id < nodes.length; id++) {
      if (nodes[id][0] !== 'a' && nodes[id][0] !== 'o') {
        continue;
      }
      if (!Array.isArray(nodes[id][1])) {
        fail('Invalid worker properties');
      }
      for (const pair of nodes[id][1]) {
        if (
          ++properties > 131072 ||
          !Array.isArray(pair) ||
          pair.length !== 2 ||
          typeof pair[0] !== 'string' ||
          (nodes[id][0] === 'a' && pair[0] === 'length') ||
          Object.hasOwn(objects[id], pair[0])
        ) {
          fail('Invalid worker property');
        }
        define(objects[id], pair[0], {
          value: unpack(pair[1]),
          enumerable: true,
          configurable: true,
          writable: true
        });
      }
    }
    return unpack(root);
  }

  return { encode, decode };
}
