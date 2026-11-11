/** Genuine on-disk SQLite workers, prepared bindings, transactions and close/reopen. */
import sqlite from 'mite:sqlite';
import assert from 'node:assert/strict';

const filename = process.argv[2];

const open = () =>
  new Promise((resolve, reject) => {
    let synchronous = true;
    const database = new sqlite.Database(filename, (error) => {
      assert.equal(synchronous, false);
      error ? reject(error) : resolve(database);
    });
    synchronous = false;
  });

const call = (database, method, ...args) =>
  new Promise((resolve, reject) => {
    database[method](...args, function (error, result) {
      error ? reject(error) : resolve(result ?? { lastID: this.lastID, changes: this.changes });
    });
  });

const database = await open();
await call(database, 'exec', 'CREATE TABLE ItemTable (key TEXT UNIQUE, value BLOB);');
await new Promise((resolve, reject) => {
  database.serialize(() => {
    database.run('BEGIN TRANSACTION');
    const statement = database.prepare('INSERT INTO ItemTable VALUES (?,?), (?,?), (?,?)');
    statement.on('error', reject);

    statement.run(
      ['text', 'persisted-α\0tail', 'bytes', Buffer.from([0, 128, 255]), 'empty', ''],
      function (error) {
        if (error) {
          reject(error);
          return;
        }
        assert.equal(this.changes, 3);
        assert.equal(typeof this.lastID, 'number');
      }
    );

    statement.finalize((error) => {
      if (error) {
        reject(error);
      }
    });

    database.run('END TRANSACTION', (error) => (error ? reject(error) : resolve()));
  });
});
await call(database, 'exec', 'BEGIN TRANSACTION');
await call(database, 'run', 'INSERT INTO ItemTable VALUES (?, ?)', [
  'rollback',
  'must-not-persist'
]);
await call(database, 'exec', 'ROLLBACK');
const rows = await call(database, 'all', 'SELECT * FROM ItemTable ORDER BY key');
assert.equal(rows.length, 3);
assert.deepEqual([...rows[0].value], [0, 128, 255]);
assert.equal(rows[1].value, '');
assert.equal(rows[2].value, 'persisted-α\0tail');
await assert.rejects(
  call(database, 'exec', 'THIS IS NOT SQL'),
  (error) => error.code === 'SQLITE_ERROR'
);
await call(database, 'close');
const reopened = await open();
const persisted = await call(reopened, 'get', 'SELECT count(*) AS total FROM ItemTable');
assert.equal(persisted.total, 3);
const version = await call(reopened, 'get', 'SELECT sqlite_version() AS version');
assert.equal(version.version, sqlite.VERSION);
await call(reopened, 'close');
console.log(
  'PASS real SQLite ' + sqlite.VERSION + ' disk bindings transaction rollback close and reopen'
);
