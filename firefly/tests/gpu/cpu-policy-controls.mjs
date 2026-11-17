/** Non-GUI argv controls; no process discovery, signals or Firefly initialization. */
import assert from 'node:assert/strict';
import { hasCpuCompositorPolicy } from './cpu-policy.mjs';

const cases = [
  [['--resource-root=/owned/share/Lagom', '--platform-features=', '--force-fontconfig'], true],
  [['--platform-features='], true],
  [['--platform-features=', '--mach-server-name=owned', '--test-mode'], true],
  [[], false],
  [['--force-cpu-painting'], false],
  [['--platform-features', 'ycbcr_import'], false],
  [['--platform-features=gpu_painting'], false],
  [['--platform-features=ycbcr_import,gpu_painting'], false],
  [['--platform-features=', '--platform-features=ycbcr_import'], false],
  [['--platform-features=', '--platform-features='], false],
  [['--platform-features=', '--platform-features', 'gpu_painting'], false],
  [['--platform-features=', '--disable-sandbox'], false],
  [['--platform-features=', '--disable-sandbox=false'], false],
  [['--platform-features=', 1], false],
  [null, false]
];
for (const [args, expected] of cases) {
  assert.equal(hasCpuCompositorPolicy(args), expected, JSON.stringify(args));
}
console.log('PASS compositor CPU argv 15 controls');
