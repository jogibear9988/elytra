/** Production JS Notification policy with a controlled peer; no OS support probes or alerts. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(
    failure instanceof Error && failure.code === code,
    'Expected ' + code + ', got ' + failure?.code
  );
}

function fixture() {
  const state = {
    handler: null,
    supported: true,
    probes: 0,
    shows: [],
    closes: [],
    exits: [],
    onShow: null,
    onSupported: null,
    onClose: null
  };
  const native = {
    appPath: process.cwd(),

    setHandler(handler) {
      state.handler = handler;
    },

    notificationSupported() {
      ++state.probes;
      state.onSupported?.();
      return state.supported;
    },

    notificationShow(token, json) {
      state.shows.push({ token, payload: JSON.parse(json) });
      state.onShow?.(token, json);
    },

    notificationClose(token) {
      state.closes.push(token);
      state.onClose?.(token);
    },

    exit(code) {
      state.exits.push(code);
    }
  };
  const api = createApi(native, EventEmitter, path, urls);
  return {
    api,
    state,

    ready() {
      state.handler(0, 'ready', '');
    },

    result(token, type, extra = {}) {
      state.handler(0, 'notification-result', JSON.stringify({ token, type, ...extra }));
    }
  };
}

const first = fixture();
const { Notification } = first.api;
check(
  Notification.isSupported() === false && first.state.probes === 0,
  'Before-ready support never probes native'
);

fails(() => new Notification(), 'ERR_FIREFLY_NOT_READY');

first.ready();
await first.api.app.whenReady();
check(
  Notification.isSupported() === true && first.state.probes === 1,
  'Controlled supported provider'
);
first.state.supported = false;
check(Notification.isSupported() === false, 'Controlled unsupported provider');
first.state.supported = true;
for (const options of [
  null,
  42,
  { title: 1 },
  { body: 42 },
  { body: null },
  { title: null },
  { title: 'nul\0text' },
  { silent: 'false' },
  { actions: {} },
  { actions: [null] },
  { actions: [{ type: 'button', text: 1 }] }
]) {
  fails(() => new Notification(options), 'ERR_INVALID_ARG_TYPE');
}
for (const options of [
  { title: 'x'.repeat(4097) },
  { title: 'ä'.repeat(2049) },
  { body: 'x'.repeat(65537) },
  { actions: Array.from({ length: 5 }, () => ({ type: 'button', text: 'Open' })) },
  { actions: [{ type: 'button', text: 'x'.repeat(1025) }] }
]) {
  fails(() => new Notification(options), 'ERR_OUT_OF_RANGE');
}
for (const options of [
  { icon: 'unsupported.png' },
  { hasReply: true },
  { unknownNotificationOption: true },
  { actions: [{ type: 'reply', text: 'Reply' }] }
]) {
  fails(() => new Notification(options), 'ERR_FIREFLY_UNSUPPORTED_API');
}
check(
  first.state.shows.length === 0 && first.state.closes.length === 0,
  'Construction/validation never shows or closes OS notifications'
);
const reads = { title: 0, body: 0, silent: 0, actions: 0, type: 0, text: 0 };
const observed = new Notification({
  get title() {
    ++reads.title;
    return 'getter';
  },

  get body() {
    ++reads.body;
    return 'body';
  },

  get silent() {
    ++reads.silent;
    return true;
  },

  get actions() {
    ++reads.actions;
    return [
      {
        get type() {
          ++reads.type;
          return 'button';
        },

        get text() {
          ++reads.text;
          return 'Open';
        }
      }
    ];
  }
});
check(
  observed.title === 'getter' && Object.values(reads).every((value) => value === 1),
  'Option/action getters are read once into owned values'
);
const getterExit = fixture();
getterExit.ready();

fails(
  () =>
    new getterExit.api.Notification({
      get title() {
        getterExit.api.app.exit(0);
        return 'exit';
      }
    }),
  'ERR_FIREFLY_APP_EXITED'
);

check(
  getterExit.state.shows.length === 0,
  'Constructor getter exit cannot submit native notification'
);
console.log('PASS notification readiness, supported-provider boundary and strict option limits');

const defaults = new Notification();
check(
  defaults.title === '' &&
    defaults.body === '' &&
    defaults.silent === false &&
    defaults.actions.length === 0,
  'Omitted fields retain empty defaults'
);
const actions = [{ type: 'button', text: 'Open' }];
const note = new Notification({ title: 'Title ä', body: 'Body', silent: true, actions });
check(
  note instanceof EventEmitter &&
    note.title === 'Title ä' &&
    note.body === 'Body' &&
    note.silent === true,
  'EventEmitter and mutable fields'
);
actions[0].text = 'outside mutation';
const copied = note.actions;
copied[0].text = 'getter mutation';
check(note.actions[0].text === 'Open', 'Constructor/getter actions own snapshots');
note.title = 'Updated';
note.body = 'Changed';
note.silent = false;
note.actions = [{ type: 'button', text: 'Next' }];

fails(() => {
  note.title = 1;
}, 'ERR_INVALID_ARG_TYPE');

fails(() => {
  note.actions = [{ type: 'button', text: 'x'.repeat(1025) }];
}, 'ERR_OUT_OF_RANGE');

check(
  note.title === 'Updated' && note.actions[0].text === 'Next',
  'Rejected setter keeps last valid fields'
);
const events = [];

note.on('show', (event) => {
  check(event && typeof event === 'object', 'Show event object');
  events.push('show');
});

note.on('close', () => events.push('close'));

note.on('click', () => events.push('click'));

check(note.show() === undefined, 'show return contract');
const original = first.state.shows.at(-1);
check(
  JSON.stringify(original.payload) ===
    JSON.stringify({
      title: 'Updated',
      body: 'Changed',
      silent: false,
      actions: [{ type: 'button', text: 'Next' }]
    }),
  'Native show receives exact validated snapshot'
);
note.title = 'Next generation';
check(
  original.payload.title === 'Updated',
  'Later mutations cannot alter submitted native metadata'
);
first.result(original.token, 'show');
first.result(original.token, 'show');
check(events.join(',') === 'show', 'Native acceptance acknowledgement emits show once');
note.show();
const replacement = first.state.shows.at(-1);
check(
  replacement.token > original.token &&
    first.state.closes.includes(original.token) &&
    events.join(',') === 'show,close',
  'Repeat show retires previous generation'
);
first.result(original.token, 'click');
first.result(original.token, 'close');
first.result(original.token, 'show');
check(events.join(',') === 'show,close', 'Old native generation cannot deliver events');
first.result(replacement.token, 'show');
first.result(replacement.token, 'click');
first.result(replacement.token, 'close');
check(events.join(',') === 'show,close,show,click', 'Terminal click retires before late close');
note.close();
check(events.join(',') === 'show,close,show,click', 'Closing an inactive generation is harmless');
console.log(
  'PASS notification field snapshots, repeat-show generations and terminal event ownership'
);

const actionable = new Notification({ actions: [{ type: 'button', text: 'Choose' }] });
const actionEvents = [];

actionable.on('action', (event, index) => actionEvents.push([event.actionIndex, index]));

actionable.show();
const actionToken = first.state.shows.at(-1).token;
first.result(actionToken, 'action', { index: 0 });
first.result(actionToken, 'action', { index: 0 });
check(JSON.stringify(actionEvents) === '[[0,0]]', 'Action event maps button index once');
const failed = new Notification();
let failure;

failed.on('failed', (event, message) => {
  check(event && typeof event === 'object', 'Failed event object');
  failure = message;
});

failed.show();
first.result(first.state.shows.at(-1).token, 'failed', { message: 'controlled provider failure' });
check(failure === 'controlled provider failure', 'Native failure reaches owned emitter');
for (const payload of [
  { type: 'action', index: 99 },
  { type: 'failed', message: 'x'.repeat(4097) },
  { type: 'unrecognized' }
]) {
  const invalidEvent = new Notification({ actions: [{ type: 'button', text: 'Only' }] });
  let failures = 0;
  let actions = 0;

  invalidEvent.on('action', () => ++actions);

  invalidEvent.on('failed', (_event, message) => {
    check(message === 'Invalid native notification event', 'Invalid native event fails explicitly');
    ++failures;
  });

  invalidEvent.show();
  const token = first.state.shows.at(-1).token;
  first.result(token, payload.type, payload);
  first.result(token, 'action', { index: 0 });
  check(failures === 1 && actions === 0, 'Malformed event retires authority before later delivery');
}
console.log('PASS notification action and failed-event payloads without OS delivery');

const reentrant = fixture();
reentrant.ready();
const closeShow = new reentrant.api.Notification({ title: 'outer' });
closeShow.show();
const oldToken = reentrant.state.shows.at(-1).token;

closeShow.once('close', () => {
  closeShow.title = 'inner';
  closeShow.show();
});

closeShow.show();
check(
  reentrant.state.shows.length === 2 &&
    reentrant.state.shows.at(-1).payload.title === 'inner' &&
    reentrant.state.closes.includes(oldToken),
  'Close-listener show supersedes outer replacement'
);
const closeClose = new reentrant.api.Notification();
closeClose.show();
const beforeClose = reentrant.state.shows.length;

closeClose.once('close', () => closeClose.close());

closeClose.show();
check(
  reentrant.state.shows.length === beforeClose,
  'Close-listener close cancels outer replacement'
);
const terminal = new reentrant.api.Notification();

terminal.on('click', () => terminal.show());

terminal.show();
const terminalToken = reentrant.state.shows.at(-1).token;
reentrant.result(terminalToken, 'click');
check(
  reentrant.state.shows.at(-1).token !== terminalToken,
  'Terminal event detaches generation before listener reentry'
);
const closeExit = fixture();
closeExit.ready();
const exitOnClose = new closeExit.api.Notification();
exitOnClose.show();

exitOnClose.once('close', () => closeExit.api.app.exit(0));

fails(() => exitOnClose.show(), 'ERR_FIREFLY_APP_EXITED');

check(
  closeExit.state.shows.length === 1 && closeExit.state.exits.length === 1,
  'Close-listener exit revokes outer replacement'
);
const nativeCloseExit = fixture();
nativeCloseExit.ready();
const exitDuringClose = new nativeCloseExit.api.Notification();
let closeAfterExit = 0;

exitDuringClose.on('close', () => ++closeAfterExit);

exitDuringClose.show();

nativeCloseExit.state.onClose = () => nativeCloseExit.api.app.exit(0);

exitDuringClose.close();
check(
  closeAfterExit === 0 && nativeCloseExit.state.exits.length === 1,
  'Native close-triggered exit suppresses retired close callback'
);
const terminalCloseExit = fixture();
terminalCloseExit.ready();
const exitDuringTerminal = new terminalCloseExit.api.Notification();
let clickAfterExit = 0;

exitDuringTerminal.on('click', () => ++clickAfterExit);

exitDuringTerminal.show();
const exitToken = terminalCloseExit.state.shows.at(-1).token;

terminalCloseExit.state.onClose = () => terminalCloseExit.api.app.exit(0);

terminalCloseExit.result(exitToken, 'click');
check(
  clickAfterExit === 0 && terminalCloseExit.state.exits.length === 1,
  'Native retirement exit suppresses terminal callback'
);
const manualReplacement = fixture();
manualReplacement.ready();
const replaceDuringManualClose = new manualReplacement.api.Notification();
let manualReplacementCloses = 0;

replaceDuringManualClose.on('close', () => ++manualReplacementCloses);

replaceDuringManualClose.show();

manualReplacement.state.onClose = () => {
  manualReplacement.state.onClose = null;
  replaceDuringManualClose.show();
};

replaceDuringManualClose.close();
check(
  manualReplacement.state.shows.length === 2 && manualReplacementCloses === 0,
  'Native close reentry preserves a new generation without notifying the retired one'
);
const nativeReplacement = fixture();
nativeReplacement.ready();
const replaceDuringNativeClose = new nativeReplacement.api.Notification();
let replacementCloses = 0;
let replacementClicks = 0;

replaceDuringNativeClose.on('close', () => ++replacementCloses);

replaceDuringNativeClose.on('click', () => ++replacementClicks);

replaceDuringNativeClose.show();
const replacedToken = nativeReplacement.state.shows.at(-1).token;

nativeReplacement.state.onClose = () => {
  nativeReplacement.state.onClose = null;
  replaceDuringNativeClose.show();
};

nativeReplacement.result(replacedToken, 'click');
check(
  nativeReplacement.state.shows.length === 2 && replacementClicks === 0 && replacementCloses === 0,
  'Native retirement reentry preserves the replacement generation without old callbacks'
);
const probeExit = fixture();
probeExit.ready();

probeExit.state.onSupported = () => probeExit.api.app.exit(0);

check(
  probeExit.api.Notification.isSupported() === false,
  'Support probe cannot return stale true after reentrant app exit'
);
console.log('PASS notification close/show and terminal-listener reentry policy');

const broken = fixture();
broken.ready();

broken.state.onShow = () => {
  throw Error('controlled native exception');
};

const brokenNote = new broken.api.Notification();
let brokenFailed = 0;

brokenNote.on('failed', (_event, message) => {
  check(message.includes('controlled native exception'), 'Native exception text');
  ++brokenFailed;
});

brokenNote.show();
check(brokenFailed === 0, 'Native show exception is reported asynchronously');
await new Promise((resolve) => setTimeout(resolve, 0));
check(brokenFailed === 1, 'Native show exception emits failed once');
const cancelledFailure = new broken.api.Notification();
let unexpectedFailed = 0;

cancelledFailure.on('failed', () => ++unexpectedFailed);

cancelledFailure.show();
cancelledFailure.close();
await new Promise((resolve) => setTimeout(resolve, 0));
check(unexpectedFailed === 0, 'Close revokes queued native failure delivery');
console.log('PASS notification native-exception retirement and queued-failure revocation');

const bounded = fixture();
bounded.ready();
const notes = Array.from({ length: 64 }, () => new bounded.api.Notification());
let shutdownCloses = 0;
for (const item of notes) {
  item.on('close', () => ++shutdownCloses);

  item.show();
}
const extra = new bounded.api.Notification();

fails(() => extra.show(), 'ERR_FIREFLY_NOTIFICATION_LIMIT');

check(bounded.state.shows.length === 64, 'Quota fails before provider');
notes[0].close();
extra.show();
check(
  bounded.state.shows.length === 65 &&
    new Set(bounded.state.shows.map(({ token }) => token)).size === 65,
  'Retired generation releases quota with fresh token'
);
const observedCloses = shutdownCloses;
bounded.api.app.exit(0);
for (const { token } of bounded.state.shows) {
  bounded.result(token, 'click');
}
check(
  shutdownCloses === observedCloses && bounded.state.closes.length === 65,
  'App shutdown closes active native generations without user callbacks'
);
const probes = bounded.state.probes;
check(
  bounded.api.Notification.isSupported() === false && bounded.state.probes === probes,
  'Exited support query does not probe native'
);

fails(() => extra.show(), 'ERR_FIREFLY_APP_EXITED');

fails(() => new bounded.api.Notification(), 'ERR_FIREFLY_APP_EXITED');

console.log('PASS notification active-generation quota, shutdown and late-event suppression');
console.log('DONE notification policy');
