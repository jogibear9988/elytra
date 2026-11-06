/** Electron 45-style asynchronous text clipboard, restricted to the main process. */
export default function createClipboard(native, error, unsupported) {
  const BlobClass = globalThis.Blob;
  const BufferClass = globalThis.Buffer;
  const entries = new WeakMap();
  const formats = ['text/plain', 'text/html', 'text/rtf'];
  const budget = 8 * 1024 * 1024;

  function supported(format) {
    if (!formats.includes(format)) {
      unsupported(`clipboard format ${format}`);
    }
  }

  function request(value) {
    const result = JSON.parse(native.clipboardRequest(JSON.stringify(value)));
    if (result.error) {
      throw error(result.error.code, result.error.message);
    }
    return result;
  }

  class ClipboardItem {
    constructor(items) {
      if (!items || typeof items !== 'object' || Array.isArray(items)) {
        throw error('ERR_INVALID_ARG_TYPE', 'ClipboardItem requires a MIME record');
      }
      const values = new Map(Object.entries(items));
      if (!values.size) {
        throw error('ERR_INVALID_ARG_VALUE', 'ClipboardItem needs at least one representation');
      }
      // Capture payload references now; resolve promised bytes only when requested.
      entries.set(this, values);
    }

    get types() {
      return [...entries.get(this).keys()];
    }

    async getType(type) {
      const values = entries.get(this);
      if (!values.has(type)) {
        throw error('ERR_INVALID_ARG_VALUE', 'ClipboardItem does not contain this type');
      }
      const value = await values.get(type);
      if (typeof value === 'string') {
        return new BlobClass([value], { type });
      }
      if (value instanceof BlobClass) {
        return value;
      }
      throw error('ERR_INVALID_ARG_TYPE', 'Clipboard payload must resolve to a string or Blob');
    }
  }

  const clipboard = {
    async readText() {
      return request({ op: 'readText' }).text;
    },

    async writeText(text) {
      if (typeof text !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'text must be a string');
      }
      await clipboard.write([new ClipboardItem({ 'text/plain': text })]);
    },

    async has(format) {
      supported(format);
      return request({ op: 'has', format }).has;
    },

    async read() {
      const { items } = request({ op: 'read' });
      return items.map(
        (item) =>
          new ClipboardItem(
            Object.fromEntries(
              item.map(({ format, data }) => [
                format,
                new BlobClass([BufferClass.from(data, 'base64')], { type: format })
              ])
            )
          )
      );
    },

    async write(items) {
      if (!Array.isArray(items) || items.length > 1) {
        throw error('ERR_INVALID_ARG_VALUE', 'clipboard.write accepts zero or one ClipboardItem');
      }
      // Validate the complete shape before awaiting payloads or changing the OS clipboard.
      const selected = items.map((item) => {
        if (!entries.has(item)) {
          throw error('ERR_INVALID_ARG_TYPE', 'Expected a Firefly ClipboardItem');
        }
        const types = item.types;
        types.forEach(supported);
        return { item, types };
      });
      let size = 0;
      const encoded = [];
      for (const { item, types } of selected) {
        const representations = [];
        for (const format of types) {
          const blob = await item.getType(format);
          size += blob.size;
          if (size > budget) {
            throw error('ERR_FIREFLY_CLIPBOARD_LIMIT', 'Clipboard data exceeds eight MiB');
          }
          const bytes = BufferClass.from(await blob.arrayBuffer());
          representations.push({ format, data: bytes.toString('base64') });
        }
        encoded.push(representations);
      }
      request({ op: 'write', items: encoded });
    },

    clear() {
      request({ op: 'clear' });
    }
  };

  return { clipboard, ClipboardItem };
}
