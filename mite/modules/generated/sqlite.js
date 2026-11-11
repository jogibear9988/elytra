/* Generated from mite/modules/storage/sqlite.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/storage/sqlite.mjs
function createSQLite(host) {
  const { native, EventEmitter, Buffer } = host;
  const enqueue = queueMicrotask;
  const registry = new FinalizationRegistry((record) => {
    try {
      native.submit(JSON.stringify(record), () => {
      });
    } catch {
    }
  });
  const error = (code, message) => Object.assign(Error(message), { code });
  const unsupported = (name) => {
    throw error("ERR_MITE_UNSUPPORTED_API", "SQLite " + name + " is not implemented");
  };
  const decodeRow = (row) => {
    const result = {};
    for (const [key, value] of Object.entries(row)) {
      Object.defineProperty(result, key, {
        value: value && typeof value === "object" ? Buffer.from(value.blob, "base64") : value,
        enumerable: true,
        writable: true,
        configurable: true
      });
    }
    return result;
  };
  function sql(value) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "SQL must be a string");
    }
    if (value.includes("\0") || Buffer.byteLength(value) > 1024 * 1024) {
      throw error("ERR_INVALID_ARG_VALUE", "SQL exceeds budget or contains NUL");
    }
    return value;
  }
  function argumentsFor(values) {
    const args = [...values];
    const callback = typeof args.at(-1) === "function" ? args.pop() : void 0;
    const parameters = args.length === 1 && Array.isArray(args[0]) ? [...args[0]] : args;
    if (parameters.length > 1024) {
      throw error("ERR_OUT_OF_RANGE", "Too many SQLite bindings");
    }
    const params = parameters.map((value) => {
      if (value === null || typeof value === "string" || typeof value === "number" && Number.isFinite(value)) {
        return value;
      }
      if (Buffer.isBuffer(value) || value instanceof Uint8Array) {
        return { blob: Buffer.from(value).toString("base64") };
      }
      throw error(
        "ERR_INVALID_ARG_TYPE",
        "SQLite bindings support null, finite numbers, strings and bytes"
      );
    });
    return { params, callback };
  }
  class Database extends EventEmitter {
    #id;
    #queue = [];
    #busy = true;
    #closed = false;
    #closing = false;
    #failure;
    constructor(filename, mode, callback) {
      super();
      if (typeof mode === "function") {
        callback = mode;
        mode = void 0;
      }
      if (typeof filename !== "string" || filename.includes("\0")) {
        throw error("ERR_INVALID_ARG_TYPE", "SQLite filename must be a NUL-free string");
      }
      if (mode !== void 0 && ![1, 2, 6].includes(mode)) {
        unsupported("open mode");
      }
      this.filename = filename;
      this.mode = mode ?? 6;
      this.open = false;
      const complete = (failure, value) => {
        this.#busy = false;
        this.#failure = failure;
        if (!failure) {
          this.#id = value.db;
          this.open = true;
          registry.register(this, { op: "dispose", db: this.#id }, this);
        }
        try {
          if (callback) {
            callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
          if (!failure) {
            this.emit("open");
          }
        } finally {
          this.#pump();
        }
      };
      this.#submit({ op: "open", path: filename, flags: this.mode }, complete);
    }
    #submit(input, complete) {
      try {
        const json = JSON.stringify(input);
        if (Buffer.byteLength(json) > 2 * 1024 * 1024) {
          throw error("ERR_MITE_SQLITE_LIMIT", "SQLite request exceeds 2 MiB");
        }
        native.submit(json, (payload) => {
          const value = JSON.parse(payload);
          const failure = value.ok ? null : Object.assign(Error(value.message), { code: value.code, errno: value.errno });
          complete(failure, value);
        });
      } catch (failure) {
        enqueue(() => complete(failure));
      }
    }
    #pump() {
      if (this.#busy || !this.#queue.length) {
        return;
      }
      const next = this.#queue.shift();
      this.#busy = true;
      const done = (failure, value) => {
        this.#busy = false;
        try {
          next.complete(failure, value);
        } finally {
          this.#pump();
        }
      };
      if (this.#failure || this.#closed) {
        enqueue(() => done(this.#failure ?? error("SQLITE_MISUSE", "Database is closed")));
        return;
      }
      try {
        this.#submit({ ...next.build(), db: this.#id }, done);
      } catch (failure) {
        enqueue(() => done(failure));
      }
    }
    _operation(build, complete) {
      if (this.#queue.length >= 128) {
        throw error("ERR_MITE_SQLITE_LIMIT", "SQLite database queue limit exceeded");
      }
      this.#queue.push({ build, complete });
      this.#pump();
    }
    _query(mode, query, values) {
      sql(query);
      if (this.#closing) {
        throw error("SQLITE_MISUSE", "Database is closing");
      }
      const { params, callback } = argumentsFor(values);
      const receiver = new EventEmitter();
      this._operation(
        () => ({ op: "query", mode, sql: query, params }),
        (failure, result) => {
          if (result) {
            receiver.lastID = result.lastID;
            receiver.changes = result.changes;
          }
          if (this.listenerCount("trace")) {
            this.emit("trace", query);
          }
          if (callback) {
            callback.call(
              receiver,
              failure,
              failure ? void 0 : mode === "get" ? result.rows[0] ? decodeRow(result.rows[0]) : void 0 : mode === "all" ? result.rows.map(decodeRow) : void 0
            );
          } else if (failure) {
            this.emit("error", failure);
          }
        }
      );
      return this;
    }
    run(query, ...values) {
      return this._query("run", query, values);
    }
    get(query, ...values) {
      return this._query("get", query, values);
    }
    all(query, ...values) {
      return this._query("all", query, values);
    }
    exec(query, callback) {
      sql(query);
      if (this.#closing) {
        throw error("SQLITE_MISUSE", "Database is closing");
      }
      this._operation(
        () => ({ op: "exec", sql: query }),
        (failure) => {
          if (this.listenerCount("trace")) {
            this.emit("trace", query);
          }
          if (callback) {
            callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
        }
      );
      return this;
    }
    prepare(query, ...values) {
      if (this.#closing) {
        throw error("SQLITE_MISUSE", "Database is closing");
      }
      return new Statement(this, sql(query), argumentsFor(values));
    }
    serialize(callback) {
      if (callback !== void 0 && typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "serialize callback must be a function");
      }
      callback?.call(this);
      return this;
    }
    parallelize() {
      return unsupported("parallelize");
    }
    configure(name, value) {
      if (name !== "busyTimeout" || !Number.isInteger(value) || value < 0 || value > 1e4) {
        unsupported("configure option");
      }
      return this.exec("PRAGMA busy_timeout=" + value);
    }
    close(callback) {
      if (this.#closing) {
        enqueue(() => callback?.call(this, error("SQLITE_MISUSE", "Database is closing")));
        return this;
      }
      this.#closing = true;
      this._operation(
        () => ({ op: "close" }),
        (failure) => {
          if (!failure) {
            this.#closed = true;
            this.open = false;
            registry.unregister(this);
          } else {
            this.#closing = false;
          }
          if (callback) {
            callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
          if (!failure) {
            this.emit("close");
          }
        }
      );
      return this;
    }
  }
  class Statement extends EventEmitter {
    #database;
    #sql;
    #id;
    #params;
    #failure;
    #finalizing = false;
    constructor(database, query, { params, callback }) {
      super();
      this.#database = database;
      this.#sql = query;
      this.#params = params;
      database._operation(
        () => ({ op: "prepare", sql: query }),
        (failure, result) => {
          this.#failure = failure;
          this.#id = result?.statement;
          if (!failure) {
            registry.register(
              this,
              { op: "finalize", db: result.db, statement: result.statement },
              this
            );
          }
          if (callback) {
            callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
        }
      );
    }
    run(...values) {
      if (this.#finalizing) {
        throw error("SQLITE_MISUSE", "Statement is finalized");
      }
      const selected = argumentsFor(values);
      const params = selected.params.length ? selected.params : this.#params;
      this.#database._operation(
        () => {
          if (this.#failure) {
            throw this.#failure;
          }
          return { op: "statement", statement: this.#id, mode: "run", params };
        },
        (failure, result) => {
          if (result) {
            this.lastID = result.lastID;
            this.changes = result.changes;
          }
          if (this.#database.listenerCount("trace")) {
            this.#database.emit("trace", this.#sql);
          }
          if (selected.callback) {
            selected.callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
        }
      );
      return this;
    }
    finalize(callback) {
      if (this.#finalizing) {
        enqueue(() => callback?.call(this, error("SQLITE_MISUSE", "Statement is finalized")));
        return this;
      }
      this.#finalizing = true;
      this.#database._operation(
        () => {
          if (this.#failure) {
            throw this.#failure;
          }
          return { op: "finalize", statement: this.#id };
        },
        (failure) => {
          if (!failure) {
            registry.unregister(this);
          }
          if (callback) {
            callback.call(this, failure);
          } else if (failure) {
            this.emit("error", failure);
          }
        }
      );
      return this;
    }
    get() {
      return unsupported("Statement.get");
    }
    all() {
      return unsupported("Statement.all");
    }
    bind() {
      return unsupported("Statement.bind");
    }
    reset() {
      return unsupported("Statement.reset");
    }
  }
  const api = {
    Database,
    Statement,
    VERSION: native.version,
    OPEN_READONLY: 1,
    OPEN_READWRITE: 2,
    OPEN_CREATE: 4,
    verbose() {
      return api;
    }
  };
  return api;
}
return createSQLite;
})()
