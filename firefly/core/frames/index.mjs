/**
 * Read-only proxies for canonical native documents. Every read revalidates the
 * owning contents and native token; neither page payloads nor IPC URLs identify
 * frames. These objects describe frames and grant no renderer capability.
 */
export default function createFrames(native, contentsAlive, error, unsupported, contentsIds) {
  const known = new Set();
  const objects = new Map();
  let stopped = false;

  const positive = (value) => Number.isInteger(value) && value > 0 && value <= 0x7fffffff;

  const text = (value) => typeof value === 'string' && value.length <= 65536;

  const destroyed = () => error('ERR_FIREFLY_OBJECT_DESTROYED', 'Render frame is no longer active');

  const invalid = () => error('ERR_FIREFLY_FRAME_DATA', 'Invalid native frame snapshot');

  function sweep() {
    // Closed owners may never be queried again. Reclaim their weak cache entries
    // during other frame reads instead of retaining one tombstone per old window.
    for (const [token, entry] of objects) {
      if (!contentsAlive(entry.contents) || !entry.object.deref()) {
        objects.delete(token);
      }
    }
    for (const contents of known) {
      if (!contentsAlive(contents)) {
        known.delete(contents);
      }
    }
  }

  function snapshot(contents) {
    if (stopped) {
      return null;
    }
    sweep();
    if (stopped || !positive(contents) || !contentsAlive(contents)) {
      return null;
    }
    if (typeof native.frames?.snapshot !== 'function') {
      unsupported('Native frame snapshots');
    }
    const wire = native.frames.snapshot(contents);
    if (stopped || !contentsAlive(contents)) {
      return null;
    }
    if (typeof wire !== 'string' || wire.length > 2 * 1024 * 1024) {
      throw invalid();
    }
    let data;
    try {
      data = JSON.parse(wire);
    } catch {
      throw invalid();
    }
    if (
      !data ||
      data.contentsId !== contents ||
      !Array.isArray(data.frames) ||
      data.frames.length > 256
    ) {
      throw invalid();
    }
    const rows = new Map();
    const documents = new Set();
    const treeIds = new Set();
    const routes = new Set();
    for (const row of data.frames) {
      if (
        !row ||
        typeof row !== 'object' ||
        !positive(row.token) ||
        rows.has(row.token) ||
        typeof row.document !== 'string' ||
        !/^[1-9]\d{0,19}:[1-9]\d{0,19}$/.test(row.document) ||
        documents.has(row.document) ||
        !positive(row.frameTreeNodeId) ||
        treeIds.has(row.frameTreeNodeId) ||
        !positive(row.processId) ||
        !positive(row.osProcessId) ||
        !positive(row.routingId) ||
        !text(row.url) ||
        !text(row.origin) ||
        !(row.parent === null || positive(row.parent)) ||
        !positive(row.top)
      ) {
        throw invalid();
      }
      const route = `${row.processId}/${row.routingId}`;
      if (routes.has(route)) {
        throw invalid();
      }
      rows.set(row.token, row);
      documents.add(row.document);
      treeIds.add(row.frameTreeNodeId);
      routes.add(route);
    }
    if (data.mainFrame === null) {
      if (rows.size !== 0) {
        throw invalid();
      }
    } else if (!positive(data.mainFrame) || !rows.has(data.mainFrame)) {
      throw invalid();
    }
    for (const row of rows.values()) {
      if (row.top !== data.mainFrame || (row.parent === null) !== (row.token === data.mainFrame)) {
        throw invalid();
      }
      const seen = new Set();
      let cursor = row;
      while (cursor.parent !== null) {
        if (seen.has(cursor.token) || seen.size >= 128 || !rows.has(cursor.parent)) {
          throw invalid();
        }
        seen.add(cursor.token);
        cursor = rows.get(cursor.parent);
      }
      if (cursor.token !== data.mainFrame) {
        throw invalid();
      }
    }
    if (!contentsIds) {
      known.add(contents);
    }
    for (const [token, entry] of objects) {
      if (entry.contents === contents && (!rows.has(token) || !entry.object.deref())) {
        objects.delete(token);
      }
    }
    return { main: data.mainFrame, rows };
  }

  function read(contents, token) {
    const data = snapshot(contents);
    const row = data?.rows.get(token);
    if (!row) {
      throw destroyed();
    }
    return { data, row };
  }

  /** The cache holds weak wrappers; retained stale proxies retain no native page. */
  function wrap(contents, token) {
    const cached = objects.get(token);
    const existing = cached?.object.deref();
    if (existing) {
      if (cached.contents !== contents) {
        throw invalid();
      }
      return existing;
    }

    const field = (name) => read(contents, token).row[name];

    const frame = Object.freeze({
      get url() {
        return field('url');
      },

      get origin() {
        return field('origin');
      },

      get processId() {
        return field('processId');
      },

      get osProcessId() {
        return field('osProcessId');
      },

      get routingId() {
        return field('routingId');
      },

      get frameTreeNodeId() {
        return field('frameTreeNodeId');
      },

      get parent() {
        const { row } = read(contents, token);
        return row.parent === null ? null : wrap(contents, row.parent);
      },

      get top() {
        return wrap(contents, field('top'));
      },

      get frames() {
        return [...read(contents, token).data.rows.values()]
          .filter((row) => row.parent === token)
          .map((row) => wrap(contents, row.token));
      },

      get framesInSubtree() {
        const { data } = read(contents, token);
        const selected = [];

        const visit = (current) => {
          selected.push(wrap(contents, current));
          for (const row of data.rows.values()) {
            if (row.parent === current) {
              visit(row.token);
            }
          }
        };

        visit(token);
        return selected;
      },

      isDestroyed() {
        try {
          return !snapshot(contents)?.rows.has(token);
        } catch {
          return true;
        }
      },

      send() {
        unsupported('WebFrameMain.send');
      },

      postMessage() {
        unsupported('WebFrameMain.postMessage');
      },

      executeJavaScript() {
        unsupported('WebFrameMain.executeJavaScript');
      },

      collectJavaScriptCallStack() {
        unsupported('WebFrameMain.collectJavaScriptCallStack');
      }
    });
    objects.set(token, { contents, object: new WeakRef(frame) });
    return frame;
  }

  function main(contents) {
    const data = snapshot(contents);
    return data?.main ? wrap(contents, data.main) : null;
  }

  function fromDocument(contents, document) {
    if (typeof document !== 'string') {
      return undefined;
    }
    const data = snapshot(contents);
    for (const row of data?.rows.values() ?? []) {
      if (row.document === document) {
        return wrap(contents, row.token);
      }
    }
    return undefined;
  }

  function fromId(process, routing) {
    if (!positive(process) || !positive(routing) || stopped) {
      return undefined;
    }
    for (const contents of contentsIds?.() ?? known) {
      if (!contentsAlive(contents)) {
        continue;
      }
      const data = snapshot(contents);
      for (const row of data?.rows.values() ?? []) {
        if (row.processId === process && row.routingId === routing) {
          return wrap(contents, row.token);
        }
      }
    }
    return undefined;
  }

  function stop() {
    stopped = true;
    known.clear();
    objects.clear();
  }

  return { main, fromDocument, fromId, stop };
}
