/** Unchanged Code-OSS service; collaborators describe windows/lifecycle, not OS hotkeys. */
export default async function exerciseConsumer(Service, shortcut, native) {
  const check = (value, message) => {
    if (!value) {
      throw Error(message);
    }
  };

  const tick = () => new Promise((resolve) => setTimeout(resolve, 10));

  const events = new Map();

  const event = (name) => (callback) => {
    events.set(name, callback);
    return {
      dispose() {
        events.delete(name);
      }
    };
  };

  const messages = [];
  const windows = new Map(
    [1, 2].map((id) => [
      id,
      {
        id,

        sendWhenReady(channel, _token, payload) {
          messages.push({ id, channel, payload });
        }
      }
    ])
  );
  let focused = windows.get(2);
  const service = new Service(
    shortcut,
    {
      getFocusedWindow: () => focused,

      getWindowById: (id) => windows.get(id),

      onDidDestroyWindow: event('destroy')
    },
    {
      onBeforeCloseWindow: event('close'),
      onWillShutdown: event('shutdown')
    },
    { warn() {}, error() {}, trace() {} }
  );
  const accelerator = 'Control+Alt+Shift+F20';
  const foreign = 'Control+Alt+Shift+F19';
  check(
    shortcut.register(foreign, () => {
      throw Error('Unrelated owner fired');
    }),
    'Separate application registration exists outside consumer ownership'
  );

  const binding = (commandId, args) => ({ accelerator, commandId, args });

  try {
    check(
      service.updateKeybindings(1, [binding('first', { version: 1 })]).failed.length === 0,
      'Code-OSS admits actual factory registration'
    );
    const registration = native.count() - 1;
    check(
      service.updateKeybindings(2, [binding('focused')]).failed.length === 0,
      'Shared registration across window owners'
    );
    check(native.count() - 1 === registration, 'Consumer shares one actual native registration');
    native.emit(registration);
    await tick();
    check(
      messages.length === 1 && messages[0].id === 2 && messages[0].payload.id === 'focused',
      'Controlled registry event routes to focused owner'
    );
    service.updateKeybindings(1, [binding('updated', { version: 2 })]);
    focused = undefined;
    native.collect?.();
    native.emit(registration);
    await tick();
    const message = messages.at(-1);
    check(
      message.id === 1 &&
        message.channel === 'vscode:runAction' &&
        message.payload.id === 'updated' &&
        message.payload.from === 'systemWideKeybinding' &&
        message.payload.args[0].version === 2,
      'Stable consumer callback reads current binding/args'
    );
    events.get('close')(windows.get(1));
    check(shortcut.isRegistered(accelerator), 'Other window retains shared registration');
    native.emit(registration);
    await tick();
    check(messages.at(-1).id === 2, 'Remaining owner receives native event');
    events.get('destroy')(windows.get(2));
    check(!shortcut.isRegistered(accelerator), 'Destroyed last owner releases actual registration');
    const before = messages.length;
    native.emit(registration);
    await tick();
    check(messages.length === before, 'Late retired native callback cannot route command');
    native.control(1);
    const failed = service.updateKeybindings(1, [binding('retry')]);
    check(failed.failed[0] === accelerator, 'Consumer reports real controlled native conflict');
    check(
      service.updateKeybindings(1, [binding('retry')]).failed.length === 0,
      'Consumer retries failed registration'
    );
    events.get('shutdown')();
    check(
      !shortcut.isRegistered(accelerator) && shortcut.isRegistered(foreign),
      'Consumer shutdown releases owned shortcuts and preserves unrelated application owner'
    );
  } finally {
    service.dispose();
    shortcut.unregister(foreign);
  }
  check(events.size === 0, 'Consumer disposes lifecycle listeners');
  console.log(
    'PASS actual pinned Code-OSS GlobalKeybindingsMainService controlled registry consumer reconciliation, event routing, current command/args, conflicts and lifecycle disposal; no OS hotkeys registered'
  );
}
