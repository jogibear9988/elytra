import {createHash} from 'node:crypto';
import {readFile} from 'node:fs/promises';

// Incremental hashing and independent snapshots use native EVP state.
const input = process.argv[2] ? await readFile(process.argv[2]) : 'abc';
const hash = createHash('sha256');
hash.update(input);
const snapshot = hash.copy();
console.log(hash.digest('hex'));
if (snapshot.digest('hex') !== createHash('sha256').update(input).digest('hex')) {
    throw Error('Hash snapshot differs');
}
