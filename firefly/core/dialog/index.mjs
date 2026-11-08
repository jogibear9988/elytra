/** Bounded dialog ownership. Native cancellation revokes delivery, not completed user actions. */
export default function createDialogs(
  native,
  isReady,
  isExited,
  resolveParent,
  error,
  unsupported
) {
  const pending = new Map();
  let next = 0;
  let stopped = false;
  const DATA = 'ERR_FIREFLY_DIALOG_DATA';

  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'Dialog application has exited');
    }
    if (ready && !isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Dialogs require app readiness');
    }
  }

  function text(value, name, maximum = 32768) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', name + ' must be a string');
    }
    for (let i = 0; i < value.length; i++) {
      const unit = value.charCodeAt(i);
      if (unit >= 0xd800 && unit <= 0xdbff) {
        const low = value.charCodeAt(++i);
        if (!(low >= 0xdc00 && low <= 0xdfff)) {
          throw error('ERR_INVALID_ARG_VALUE', name + ' contains an unpaired surrogate');
        }
      } else if (unit >= 0xdc00 && unit <= 0xdfff) {
        throw error('ERR_INVALID_ARG_VALUE', name + ' contains an unpaired surrogate');
      }
    }
    if (value.includes('\0')) {
      throw error('ERR_INVALID_ARG_VALUE', name + ' contains NUL');
    }
    if (Buffer.byteLength(value, 'utf8') > maximum) {
      throw error('ERR_OUT_OF_RANGE', name + ' exceeds its UTF-8 budget');
    }
    return value;
  }

  function boolean(value, name) {
    if (typeof value !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', name + ' must be a boolean');
    }
    return value;
  }

  function integer(value, name, min, max) {
    if (!Number.isInteger(value) || value < min || value > max) {
      throw error('ERR_OUT_OF_RANGE', name + ' is out of range');
    }
    return value;
  }

  function record(value, label) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', label + ' must be an object');
    }
  }

  function normalize(kind, source) {
    if (source === undefined || source === null) {
      source = kind === 'open' ? { title: 'Open' } : kind === 'save' ? { title: 'Save' } : {};
    }
    record(source, 'Dialog options');
    const common = ['title', 'message'];
    const keys =
      kind === 'message'
        ? [
            ...common,
            'type',
            'detail',
            'buttons',
            'defaultId',
            'cancelId',
            'checkboxLabel',
            'checkboxChecked',
            'noLink',
            'normalizeAccessKeys'
          ]
        : [
            ...common,
            'buttonLabel',
            'defaultPath',
            'filters',
            'properties',
            ...(kind === 'save' ? ['nameFieldLabel', 'showsTagField'] : [])
          ];
    for (const key of Reflect.ownKeys(source)) {
      if (!keys.includes(key)) {
        unsupported('dialog option ' + String(key));
      }
    }
    // Read each caller property once, then validate only the owned snapshot.
    const input = {};
    for (const key of keys) {
      input[key] = source[key];
    }
    const output = {};
    const strings =
      kind === 'message'
        ? ['title', 'message', 'detail', 'checkboxLabel']
        : [
            'title',
            'message',
            'buttonLabel',
            'defaultPath',
            ...(kind === 'save' ? ['nameFieldLabel'] : [])
          ];
    for (const key of strings) {
      output[key] = text(input[key] === undefined ? '' : input[key], key);
    }
    if (kind === 'message') {
      output.type = input.type === undefined ? 'none' : input.type;
      if (!['none', 'info', 'error', 'question', 'warning'].includes(output.type)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid message box type');
      }
      const buttons = input.buttons === undefined ? ['OK'] : input.buttons;
      if (!Array.isArray(buttons) || buttons.length > 32) {
        throw error('ERR_INVALID_ARG_TYPE', 'buttons must be an array with at most 32 entries');
      }
      output.buttons = Array.from(buttons, (button) => text(button, 'button', 1024));
      if (!output.buttons.length) {
        output.buttons.push('OK');
      }
      output.defaultId = integer(
        input.defaultId === undefined ? -1 : input.defaultId,
        'defaultId',
        -1,
        output.buttons.length - 1
      );
      let cancelId = input.cancelId;
      if (cancelId === undefined || cancelId === null) {
        cancelId = output.defaultId === 0 && output.buttons.length > 1 ? 1 : 0;
        const found = output.buttons.findIndex((button) =>
          ['cancel', 'no'].includes(button.toLowerCase())
        );
        if (found !== -1) {
          cancelId = found;
        }
      }
      output.cancelId = integer(cancelId, 'cancelId', 0, output.buttons.length - 1);
      for (const key of ['checkboxChecked', 'noLink', 'normalizeAccessKeys']) {
        output[key] = boolean(input[key] === undefined ? false : input[key], key);
      }
      if (output.checkboxChecked && !output.checkboxLabel) {
        throw error('ERR_INVALID_ARG_VALUE', 'checkboxChecked requires checkboxLabel');
      }
    } else {
      const properties =
        input.properties === undefined ? (kind === 'open' ? ['openFile'] : []) : input.properties;
      const allowed =
        kind === 'open'
          ? [
              'openFile',
              'openDirectory',
              'multiSelections',
              'showHiddenFiles',
              'createDirectory',
              'noResolveAliases',
              'treatPackageAsDirectory'
            ]
          : ['showHiddenFiles', 'createDirectory', 'treatPackageAsDirectory'];
      if (!Array.isArray(properties) || properties.length > 16) {
        throw error('ERR_INVALID_ARG_TYPE', 'properties must be an array');
      }
      output.properties = Array.from(properties, (property) => {
        if (!allowed.includes(property)) {
          unsupported('dialog property ' + String(property));
        }
        return property;
      });
      if (
        kind === 'open' &&
        !output.properties.some(
          (property) => property === 'openFile' || property === 'openDirectory'
        )
      ) {
        throw error('ERR_INVALID_ARG_VALUE', 'Open dialog requires a file or directory selector');
      }
      const filters = input.filters === undefined ? [] : input.filters;
      if (!Array.isArray(filters) || filters.length > 64) {
        throw error('ERR_INVALID_ARG_TYPE', 'filters must be an array with at most 64 entries');
      }
      output.filters = Array.from(filters, (filter) => {
        record(filter, 'Filter');
        for (const key of Reflect.ownKeys(filter)) {
          if (key !== 'name' && key !== 'extensions') {
            unsupported('dialog filter ' + String(key));
          }
        }
        const name = text(filter.name, 'filter name');
        const extensions = filter.extensions;
        if (!Array.isArray(extensions) || !extensions.length || extensions.length > 64) {
          throw error('ERR_INVALID_ARG_TYPE', 'extensions must contain 1 to 64 entries');
        }
        return {
          name,
          extensions: Array.from(extensions, (extension) => {
            const value = text(extension, 'extension', 256);
            if (!value || (value !== '*' && /[.*\\/]/.test(value))) {
              throw error('ERR_INVALID_ARG_VALUE', 'Invalid file extension');
            }
            return value;
          })
        };
      });
      if (kind === 'save') {
        output.showsTagField = boolean(
          input.showsTagField === undefined ? true : input.showsTagField,
          'showsTagField'
        );
      }
    }
    const json = JSON.stringify(output);
    if (Buffer.byteLength(json, 'utf8') > 262144) {
      throw error('ERR_OUT_OF_RANGE', 'Dialog options exceed their JSON budget');
    }
    return { options: output, json };
  }

  function parentId(parent) {
    if (parent == null) {
      return 0;
    }
    const id = resolveParent(parent);
    if (!Number.isInteger(id) || id <= 0 || id > 2147483647) {
      throw error('ERR_INVALID_ARG_TYPE', 'Parent must be a live BrowserWindow');
    }
    return id;
  }

  function prepare(kind, args, ready = true) {
    alive(ready);
    let parent;
    let options;
    if (args.length > 2) {
      throw error('ERR_INVALID_ARG_TYPE', 'Too many dialog arguments');
    }
    if (args.length === 2) {
      parent = args[0];
      options = args[1];
    } else {
      const first = args[0];
      if (first != null) {
        const id = resolveParent(first);
        if (id === undefined) {
          options = first;
        } else {
          parent = first;
        }
      }
    }
    const snapshot = normalize(kind, options);
    const id = parentId(parent);
    alive(ready);
    return { ...snapshot, parent, parentId: id };
  }

  function cancel(id) {
    try {
      native.dialogCancel?.(id);
    } catch {
      /* Continue retiring every request. */
    }
  }

  function request(kind, args) {
    const snapshot = prepare(kind, args);
    if (pending.size >= 16 || next >= 2147483647) {
      throw error('ERR_FIREFLY_DIALOG_LIMIT', 'Dialog request budget exhausted');
    }
    const id = ++next;
    return new Promise((resolve, reject) => {
      const entry = {
        kind,
        options: snapshot.options,
        resolve,
        reject,
        admitted: false,
        early: undefined
      };
      pending.set(id, entry);
      try {
        const start = native.dialogStart;
        if (
          typeof start !== 'function' ||
          typeof native.dialogCancel !== 'function' ||
          typeof native.dialogShutdown !== 'function'
        ) {
          unsupported('dialog native ownership');
        }
        if (parentId(snapshot.parent) !== snapshot.parentId) {
          throw error('ERR_INVALID_ARG_VALUE', 'Dialog parent identity changed');
        }
        alive();
        start.call(native, id, kind, snapshot.parentId, snapshot.json);
        if (pending.get(id) !== entry || stopped || isExited()) {
          cancel(id);
          alive();
          return;
        }
        if (parentId(snapshot.parent) !== snapshot.parentId) {
          throw error('ERR_INVALID_ARG_VALUE', 'Dialog parent identity changed');
        }
        alive();
        entry.admitted = true;
        if (entry.early !== undefined) {
          queueMicrotask(() => dispatch(entry.early));
        }
      } catch (failure) {
        if (pending.get(id) === entry) {
          pending.delete(id);
          cancel(id);
          reject(failure);
        } else {
          cancel(id);
        }
      }
    });
  }

  function selectedPath(value) {
    const path = text(value, 'Selected path');
    // Preserve native spelling: validate roots without resolving or canonicalizing.
    if (
      !path.startsWith('/') &&
      !/^[a-z]:[\\/]/i.test(path) &&
      !/^\\\\[^\\/]+[\\/][^\\/]+(?:[\\/]|$)/.test(path)
    ) {
      throw Error('Selected path must be absolute');
    }
    return path;
  }

  function result(entry, value) {
    const snapshot = resultSnapshot(entry, value);
    if (Buffer.byteLength(JSON.stringify(snapshot), 'utf8') > 262144) {
      throw Error('Dialog result exceeds its JSON budget');
    }
    return snapshot;
  }

  function resultSnapshot(entry, value) {
    record(value, 'Dialog result');
    const keys =
      entry.kind === 'message'
        ? ['response', 'checkboxChecked']
        : entry.kind === 'open'
          ? ['canceled', 'filePaths']
          : ['canceled', 'filePath'];
    if (Reflect.ownKeys(value).some((key) => !keys.includes(key))) {
      throw Error('Unknown dialog result field');
    }
    if (entry.kind === 'message') {
      return {
        response: integer(value.response, 'response', 0, entry.options.buttons.length - 1),
        checkboxChecked: boolean(value.checkboxChecked, 'checkboxChecked')
      };
    }
    const canceled = boolean(value.canceled, 'canceled');
    if (entry.kind === 'open') {
      const paths = value.filePaths;
      if (!Array.isArray(paths) || paths.length > 256) {
        throw Error('Invalid selected paths');
      }
      const filePaths = Array.from(paths, selectedPath);
      if ((canceled && filePaths.length) || (!canceled && !filePaths.length)) {
        throw Error('Inconsistent open result');
      }
      if (!entry.options.properties.includes('multiSelections') && filePaths.length > 1) {
        throw Error('Unexpected multiple selection');
      }
      return { canceled, filePaths };
    }
    const filePath = canceled
      ? text(value.filePath, 'Selected path')
      : selectedPath(value.filePath);
    if (canceled !== (filePath === '')) {
      throw Error('Inconsistent save result');
    }
    return { canceled, filePath };
  }

  function dispatch(packet) {
    const entry = pending.get(packet?.id);
    if (!entry || stopped || isExited()) {
      return;
    }
    if (!entry.admitted) {
      // Only one terminal completion may be queued during native admission.
      if (entry.early !== undefined) {
        pending.delete(packet.id);
        cancel(packet.id);
        entry.reject(error(DATA, 'Duplicate early dialog completion'));
      } else {
        try {
          entry.early = JSON.parse(JSON.stringify(packet));
        } catch {
          pending.delete(packet.id);
          cancel(packet.id);
          entry.reject(error(DATA, 'Invalid dialog completion'));
        }
      }
      return;
    }
    try {
      const hasError = Object.hasOwn(packet, 'error');
      if (hasError === Object.hasOwn(packet, 'value')) {
        throw Error('Ambiguous dialog completion');
      }
      let failure;
      let value;
      if (hasError) {
        record(packet.error, 'Dialog error');
        const code = text(packet.error.code, 'Error code', 256);
        const message = text(packet.error.message, 'Error message');
        failure = error(code, message);
      } else {
        value = result(entry, packet.value);
      }
      alive();
      if (pending.get(packet.id) !== entry) {
        return;
      }
      pending.delete(packet.id);
      if (failure) {
        entry.reject(failure);
      } else {
        entry.resolve(value);
      }
    } catch {
      if (pending.get(packet.id) !== entry) {
        return;
      }
      pending.delete(packet.id);
      cancel(packet.id);
      entry.reject(error(DATA, 'Invalid native dialog completion'));
    }
  }

  function sync(args, ready) {
    const snapshot = prepare('message', args, ready);
    const show = native.dialogMessageSync;
    if (typeof show !== 'function') {
      return unsupported('dialog.showMessageBoxSync native provider');
    }
    if (parentId(snapshot.parent) !== snapshot.parentId) {
      throw error('ERR_INVALID_ARG_VALUE', 'Dialog parent identity changed');
    }
    alive(ready);
    const raw = show.call(native, snapshot.parentId, snapshot.json);
    alive(ready);
    if (parentId(snapshot.parent) !== snapshot.parentId) {
      throw error('ERR_INVALID_ARG_VALUE', 'Dialog parent identity changed');
    }
    try {
      return result({ kind: 'message', options: snapshot.options }, JSON.parse(raw)).response;
    } catch {
      throw error(DATA, 'Invalid synchronous dialog result');
    }
  }

  function receiver(value) {
    if (value !== dialog) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal dialog receiver');
    }
  }

  const dialog = {
    showMessageBox(...args) {
      receiver(this);
      return request('message', args);
    },

    showOpenDialog(...args) {
      receiver(this);
      return request('open', args);
    },

    showSaveDialog(...args) {
      receiver(this);
      return request('save', args);
    },

    showMessageBoxSync(...args) {
      receiver(this);
      return sync(args, true);
    },

    showErrorBox(title, content) {
      receiver(this);
      sync(
        [{ type: 'error', title: text(title, 'title'), message: text(content, 'content') }],
        false
      );
    },

    showOpenDialogSync() {
      receiver(this);
      return unsupported('dialog.showOpenDialogSync');
    },

    showSaveDialogSync() {
      receiver(this);
      return unsupported('dialog.showSaveDialogSync');
    },

    showCertificateTrustDialog() {
      receiver(this);
      return unsupported('dialog.showCertificateTrustDialog');
    }
  };

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const entries = [...pending];
    pending.clear();
    for (const [id, entry] of entries) {
      entry.reject(error('ERR_FIREFLY_APP_EXITED', 'Dialog application has exited'));
      cancel(id);
    }
    try {
      native.dialogShutdown?.();
    } catch {
      /* Shutdown remains permanent. */
    }
  }

  return { dialog, dispatch, stop };
}
