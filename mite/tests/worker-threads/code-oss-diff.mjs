/** Optional real CodeOSS diff gate; requires the pinned, fully verified compiled stage. */
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
const stage = process.argv[2];
assert.ok(
  typeof stage === 'string' && path.isAbsolute(stage),
  'Pass the verified absolute CodeOSS stage'
);
const metadata = JSON.parse(readFileSync(path.join(stage, 'ELYTRA-COMPILE.json'), 'utf8'));
assert.equal(metadata.adaptations.revision, '6531648f5ace8b77ae799262bdc97bf048461938');
for (const [source, sha] of [
  ['diffWorkerMain.ts', '5bf6f29fd4424c5a0eee3946f5ef800d6c8762ebaee60e4eec6a14723fd43197'],
  ['diffComputeService.ts', 'd33579abe20a405fdc4737d3e7ef2367eadbc41b7d5854b5fe30f77a78bf74de']
]) {
  const record = metadata.inputs.find(
    (item) => item.source === 'src/vs/platform/agentHost/node/' + source
  );
  assert.ok(record, 'Compiled original diff input');
  assert.equal(record.sourceSha256, sha);
  assert.equal(
    createHash('sha256')
      .update(readFileSync(path.join(stage, record.output)))
      .digest('hex'),
    record.outputSha256
  );
}
const worker = new Worker(path.join(stage, 'src/vs/platform/agentHost/node/diffWorkerMain.js'), {
  name: 'Original CodeOSS diff'
});
const deadline = setTimeout(() => {
  throw Error('Original CodeOSS diff worker timed out');
}, 15000);
try {
  const result = await new Promise((resolve, reject) => {
    worker.once('error', reject);
    worker.once('message', resolve);
    worker.postMessage({ id: 1, fn: 'computeDiffCounts', args: ['old\n', 'new\nextra\n', 1000] });
  });
  assert.equal(result.id, 1);
  assert.equal(result.err, undefined);
  assert.equal(result.res.added, 2);
  assert.equal(result.res.removed, 1);
  assert.equal(result.res.hitTimeout, false);
  let offset = 0;
  let reconstructed = '';
  for (const change of result.res.changes) {
    reconstructed += 'old\n'.slice(offset, change.startOffset) + change.newText;
    offset = change.endOffsetExclusive;
  }
  reconstructed += 'old\n'.slice(offset);
  assert.equal(reconstructed, 'new\nextra\n');
  console.log('PASS original CodeOSS diff worker calculates real changes over a native thread');
} finally {
  clearTimeout(deadline);
  await worker.terminate();
}
