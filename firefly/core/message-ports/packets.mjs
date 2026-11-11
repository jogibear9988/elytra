/** Decode private utility envelopes without exposing native route descriptors to application code. */
const ByteArray = Uint8Array;
const apply = Reflect.apply;
const typed = Object.getPrototypeOf(ByteArray.prototype);
const getters = Object.getOwnPropertyDescriptors(typed);
const tag = getters[Symbol.toStringTag].get;
const define = Object.defineProperty;
const maxFrame = 128 * 1024;

function invalid() {
  throw Object.assign(new Error('Invalid utility port packet'), {
    code: 'ERR_FIREFLY_UTILITY_CHANNEL'
  });
}

export default function decodePortPacket(value, expected) {
  let bytes;
  let size;
  let buffer;
  let baseOffset;
  try {
    if (apply(tag, value, []) !== 'Uint8Array') {
      invalid();
    }
    size = apply(getters.byteLength.get, value, []);
    if (size < 16 || size > maxFrame) {
      invalid();
    }
    buffer = apply(getters.buffer.get, value, []);
    baseOffset = apply(getters.byteOffset.get, value, []);
    bytes = new ByteArray(buffer, baseOffset, size);
  } catch {
    invalid();
  }
  if (
    bytes[0] !== 70 ||
    bytes[1] !== 85 ||
    bytes[2] !== 80 ||
    bytes[3] !== 49 ||
    bytes[6] !== 0 ||
    bytes[7] !== 0
  ) {
    invalid();
  }

  const number = (offset) =>
    bytes[offset] * 16777216 +
    bytes[offset + 1] * 65536 +
    bytes[offset + 2] * 256 +
    bytes[offset + 3];

  const positive = (value) => value > 0 && value <= 2147483647;

  const kind = bytes[4];
  const count = bytes[5];
  const id = number(8);
  const generation = number(12);
  const required =
    expected === 'port-transfer'
      ? 1
      : expected === 'port-data'
        ? 2
        : expected === 'port-close'
          ? 3
          : 0;
  if (!required || kind !== required) {
    invalid();
  }
  const descriptors = [];
  let offset = 16;
  if (kind === 1) {
    if (!count || count > 16 || id || generation || size <= offset + count * 8) {
      invalid();
    }
    for (let index = 0; index < count; index++) {
      const portId = number(offset);
      const portGeneration = number(offset + 4);
      if (!positive(portId) || !positive(portGeneration)) {
        invalid();
      }
      for (let previous = 0; previous < index; previous++) {
        if (descriptors[previous].id === portId) {
          invalid();
        }
      }
      define(descriptors, `${index}`, {
        value: { id: portId, generation: portGeneration },
        writable: true,
        configurable: true,
        enumerable: true
      });
      offset += 8;
    }
  } else if (
    count ||
    !positive(id) ||
    !positive(generation) ||
    (kind === 3 ? size !== 16 : size <= 16)
  ) {
    invalid();
  }
  return {
    id,
    generation,
    descriptors,
    wire: new ByteArray(buffer, baseOffset + offset, size - offset)
  };
}
