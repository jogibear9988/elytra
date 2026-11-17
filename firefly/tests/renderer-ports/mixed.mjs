/** Exercise real mixed logical admission in the existing hidden offscreen batch. */
import { MessageChannelMain } from 'firefly';

export default async function runMixed({ current, replace, page, reports, check, until }) {
  const report = (batch) =>
    reports.find((value) => value.kind === 'mixed-ordinary' && value.batch === batch);

  async function ordinaryReport(batch, count) {
    if (count >= 16) {
      await until(() =>
        reports.some(
          (value) =>
            value.kind === 'mixed-progress' &&
            value.batch === batch &&
            value.count >= Math.floor(count / 16) * 16
        )
      );
    }
    current().webContents.send('mixed:report', batch);
    await until(() => report(batch));
    check(
      report(batch).count === count && report(batch).ordered,
      'Every mixed ordinary event arrives exactly once/FIFO'
    );
  }

  function exchange(phase) {
    const pair = new MessageChannelMain();
    const state = { pair, data: [], closed: false };

    pair.port1.on('message', ({ data }) => state.data.push(data));

    pair.port1.once('close', () => {
      state.closed = true;
    });

    pair.port1.start();
    // Real queued data must survive logical admission before the SDK pair exists.
    pair.port1.postMessage({ kind: 'ping', nonce: phase });
    return state;
  }

  async function finish(state, phase) {
    await until(() => state.data.length === 1);
    check(
      state.data[0].kind === 'pong' && state.data[0].nonce === phase,
      'Actual DOM duplex nonce'
    );
    state.pair.port1.postMessage({ kind: 'finish', nonce: phase });
    await until(
      () =>
        state.closed &&
        reports.some((value) => value.kind === 'mixed-open' && value.phase === phase)
    );
    check(
      state.data.length === 2 && state.data[1].kind === 'tail' && state.data[1].nonce === phase,
      'Accepted DOM tail precedes close exactly once'
    );
    check(
      reports.filter((value) => value.kind === 'mixed-open' && value.phase === phase).length === 1,
      'One genuine port handoff'
    );
  }

  const first = exchange('mixed-window');
  // No owner-thread yield: ordinary ACKs cannot free this native window yet.
  for (let index = 0; index < 16; index++) {
    current().webContents.send('mixed:ordinary', 'first', index);
  }
  current().webContents.postMessage('mixed:handoff', 'mixed-window', [first.pair.port2]);
  first.pair.port2.postMessage({ kind: 'detached' });
  first.pair.port2.close();
  await finish(first, 'mixed-window');
  await ordinaryReport('first', 16);
  console.log(
    'PASS renderer ports: full ordinary window admits queued real DOM handoff without retry'
  );

  let count = 0;
  let full = false;
  for (let index = 0; index <= 272; index++) {
    try {
      current().webContents.send('mixed:ordinary', 'full', index);
      count++;
    } catch (error) {
      check(error.code === 'ERR_FIREFLY_IPC_LIMIT', 'Only actual finite waiting capacity refuses');
      full = true;
      break;
    }
  }
  check(full && count >= 256 && count <= 272, 'Existing finite ordinary backlog is actually full');
  const victim = new MessageChannelMain();
  let refused = false;
  try {
    current().webContents.postMessage('mixed:handoff', 'must-refuse', [victim.port2]);
  } catch (error) {
    refused = error.code === 'ERR_FIREFLY_PORT_CHANNEL';
  }
  check(refused, 'Full shared waiting queue rejects transfer before detach');
  let intact = false;

  victim.port2.once('message', ({ data }) => {
    intact = data === 'still-owned';
  });

  victim.port2.start();
  victim.port1.postMessage('still-owned');
  await until(() => intact);
  victim.port1.close();
  victim.port2.close();
  await ordinaryReport('full', count);
  const recovered = exchange('mixed-recovered');
  current().webContents.postMessage('mixed:handoff', 'mixed-recovered', [recovered.pair.port2]);
  await finish(recovered, 'mixed-recovered');
  check(
    !reports.some((value) => value.phase === 'must-refuse'),
    'Rejected transfer never reaches DOM'
  );
  console.log(
    'PASS renderer ports: finite mixed backlog rejects atomically then drains and recovers'
  );

  const stale = exchange('mixed-old-epoch');
  for (let index = 0; index < 16; index++) {
    current().webContents.send('mixed:ordinary', 'closing', index);
  }
  current().webContents.postMessage('mixed:handoff', 'mixed-old-epoch', [stale.pair.port2]);
  const old = current();
  old.destroy();
  await until(() => old.isDestroyed() && stale.closed);
  check(stale.data.length === 0, 'Queued transfer canceled before old owner closes');
  const next = replace();
  await next.loadURL(page + '?mixed=new');
  await until(() =>
    reports.some((value) => value.kind === 'mixed-ready' && value.epoch === '?mixed=new')
  );
  const fresh = exchange('mixed-new-epoch');
  next.webContents.postMessage('mixed:handoff', 'mixed-new-epoch', [fresh.pair.port2]);
  await finish(fresh, 'mixed-new-epoch');
  check(
    !reports.some((value) => value.kind === 'mixed-open' && value.phase === 'mixed-old-epoch'),
    'Old queued port never crosses document ownership'
  );
  console.log(
    'PASS renderer ports: queued old owner close retires ports and fresh document remains usable'
  );
}
