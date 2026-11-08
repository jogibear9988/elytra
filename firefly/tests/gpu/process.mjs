/** One real owned compositor termination. No foreign PID, visible window, input or synthetic exit. */
import { app, BrowserWindow } from 'firefly';
import { execFile } from 'node:child_process';
import path from 'node:path';

const directory = import.meta.dirname;
app.setPath('userData', path.join(directory, 'config'));
const CPU = { gpu_compositing: 'disabled_software', rasterization: 'disabled_software' };

const pause = () => new Promise((resolve) => setTimeout(resolve, 50));

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

let stage = 'readiness';
let window;
let killed = false;
let deadline;
const exits = [];

app.on('child-process-gone', (_event, details) => {
  if (details.type === 'GPU') {
    exits.push({ ...details });
  }
});
// Keep teardown explicit: closing this sole hidden window is not a test result.
app.on('window-all-closed', () => {});

async function ownedCompositors() {
  check(Date.now() < deadline, 'Deadline expired while ' + stage);
  const output = await new Promise((resolve, reject) => {
    execFile(
      '/bin/ps',
      ['-axo', 'pid=,ppid=,args='],
      {
        encoding: 'utf8',
        timeout: 1000,
        maxBuffer: 2097152
      },
      (failure, stdout) => (failure ? reject(failure) : resolve(stdout))
    );
  });
  const owned = [];
  for (const line of output.split('\n')) {
    const match = /^\s*(\d+)\s+(\d+)\s+(.+)$/.exec(line);
    if (!match || Number(match[2]) !== process.pid) {
      continue;
    }
    const words = match[3].trim().split(/\s+/);
    if (path.basename(words[0]) !== 'Compositor') {
      continue;
    }
    const pid = Number(match[1]);
    check(
      Number.isSafeInteger(pid) && pid > 1 && pid !== process.pid,
      'Invalid owned compositor PID'
    );
    owned.push({ pid, executable: words[0], args: words.slice(1) });
  }
  return owned;
}

async function waitFor(probe, description) {
  stage = description;
  while (Date.now() < deadline) {
    const value = await probe();
    if (value) {
      return value;
    }
    await pause();
  }
  throw Error('Timed out while ' + description);
}

app.whenReady().then(async () => {
  deadline = Date.now() + 10000;
  const watchdog = setTimeout(() => {
    console.error('Owned compositor deadline expired at ' + stage);
    app.exit(1);
  }, 11000);
  let status = 1;
  try {
    check(
      ['darwin', 'linux'].includes(process.platform),
      'Only genuine macOS/Linux POSIX acceptance'
    );
    check(
      JSON.stringify(app.getGPUFeatureStatus()) === JSON.stringify(CPU),
      'Actual force-CPU policy selected'
    );
    stage = 'hidden local page load';
    window = new BrowserWindow({ show: false, width: 120, height: 80 });
    await window.loadFile(path.join(directory, 'owned.html'));
    const owner = await waitFor(async () => {
      const candidates = await ownedCompositors();
      check(candidates.length <= 1, 'Ambiguous direct owned compositor; refuse all signals');
      return candidates[0];
    }, 'discovering one direct owned compositor');
    check(
      owner.args.includes('--force-cpu-painting'),
      'Actual compositor arguments select CPU painting'
    );
    // Re-read ownership immediately before signalling. Never select by name alone.
    const verified = (await ownedCompositors()).filter((entry) => entry.pid === owner.pid);
    check(
      verified.length === 1 &&
        verified[0].executable === owner.executable &&
        verified[0].args.includes('--force-cpu-painting'),
      'Owned PID identity changed; refuse signal'
    );
    check(!killed, 'Only one owned compositor may be terminated');
    killed = true;
    // process.kill is not yet a Mite API. Use the actual POSIX utility with
    // direct numeric arguments after the same strict owned-child verification.
    await new Promise((resolve, reject) => {
      execFile('/bin/kill', ['-9', String(owner.pid)], { timeout: 1000 }, (failure) =>
        failure ? reject(failure) : resolve()
      );
    });
    const details = await waitFor(
      () => exits.find((entry) => entry.pid === owner.pid),
      'genuine GPU child-process-gone'
    );
    check(
      details.type === 'GPU' && details.reason === 'killed' && details.exitCode === 137,
      'Real SIGKILL exit classification: ' + JSON.stringify(details)
    );
    check(
      JSON.stringify(app.getGPUFeatureStatus()) === JSON.stringify(CPU),
      'Confirmed compositor policy survives child exit'
    );
    const replacement = await waitFor(async () => {
      const children = await ownedCompositors();
      return children.find((entry) => entry.pid !== owner.pid);
    }, 'native compositor replacement');
    check(
      replacement.args.includes('--force-cpu-painting'),
      'Replacement preserves actual CPU policy'
    );
    check(
      exits.filter((entry) => entry.pid === owner.pid).length === 1,
      'Single genuine termination event'
    );
    console.log(
      'PASS real owned compositor SIGKILL GPU event PID reason exitCode and native replacement; no synthetic recovery info'
    );
    status = 0;
  } catch (failure) {
    console.error(
      'GPU owned-process acceptance failed at ' + stage + ': ' + (failure.stack ?? failure)
    );
  } finally {
    clearTimeout(watchdog);
    try {
      window?.destroy();
    } finally {
      app.exit(status);
    }
  }
});
