/** Canonical single-key Electron menu accelerators; native Qt owns actual key delivery. */
export default function createAccelerators(error, unsupported, platform) {
  const modifiers = new Map([
    ['shift', 1],
    ['control', 2],
    ['ctrl', 2],
    ['alt', 4],
    ['commandorcontrol', platform === 'darwin' ? 8 : 2],
    ['cmdorctrl', platform === 'darwin' ? 8 : 2],
    ['super', 8],
    ['meta', 8]
  ]);
  if (platform === 'darwin') {
    modifiers.set('command', 8);
    modifiers.set('cmd', 8);
    modifiers.set('option', 4);
  }
  const names = new Map([
    ['space', 'Space'],
    ['tab', 'Tab'],
    ['enter', 'Enter'],
    ['return', 'Enter'],
    ['escape', 'Escape'],
    ['esc', 'Escape'],
    ['backspace', 'Backspace'],
    ['delete', 'Delete'],
    ['insert', 'Insert'],
    ['home', 'Home'],
    ['end', 'End'],
    ['pageup', 'PageUp'],
    ['pagedown', 'PageDown'],
    ['up', 'Up'],
    ['down', 'Down'],
    ['left', 'Left'],
    ['right', 'Right'],
    ['plus', '+']
  ]);
  return (value) => {
    if (value === undefined || value === null) {
      return null;
    }
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'accelerator must be a string or null');
    }
    if (!value || value.length > 256 || /[^\x20-\x7e]/.test(value)) {
      throw error('ERR_INVALID_ARG_VALUE', 'accelerator must be a bounded ASCII chord');
    }
    const parts = value === '+' ? ['Plus'] : value.split('+').map((part) => part.trim());
    if (parts.length >= 2 && parts.at(-1) === '' && parts.at(-2) === '') {
      parts.splice(-2, 2, 'Plus');
    }
    let key;
    let flags = 0;
    for (const part of parts) {
      const lower = part.toLowerCase();
      if (modifiers.has(lower)) {
        flags |= modifiers.get(lower);
        continue;
      }
      if (
        [
          'command',
          'cmd',
          'option',
          'altgr',
          'capslock',
          'numlock',
          'scrolllock',
          'printscreen'
        ].includes(lower) ||
        /^(media|volume|num)/.test(lower)
      ) {
        unsupported('Menu accelerator platform/key ' + part);
      }
      let current = names.get(lower);
      if (!current && /^[a-z0-9]$/i.test(part)) {
        current = part.toUpperCase();
      }
      if (!current && /^f(?:[1-9]|1[0-9]|2[0-4])$/i.test(part)) {
        current = part.toUpperCase();
      }
      if (!current && part.length === 1 && /^[!"#$%&'()*,\-./:;<=>?@[\]\\^_`{|}~]$/.test(part)) {
        current = part;
      }
      if (!current || key !== undefined) {
        throw error('ERR_INVALID_ARG_VALUE', 'accelerator needs exactly one supported key');
      }
      key = current;
    }
    if (!key) {
      throw error('ERR_INVALID_ARG_VALUE', 'accelerator needs a key');
    }
    return { key, modifiers: flags };
  };
}
