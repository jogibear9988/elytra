/** Promise API over measured native main events and Mite's asynchronous filesystem. */
export default function createTracing(native, isReady, files, paths, error, unsupported) {
  let state = 'idle';
  let pending;
  let temporary;

  function ready() {
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'contentTracing requires app readiness');
    }
  }

  function object(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'Tracing options must be an object');
    }
  }

  function categories(value, available) {
    if (!Array.isArray(value) || value.length > 32) {
      throw error(
        'ERR_INVALID_ARG_VALUE',
        'Tracing categories must be an array of at most 32 entries'
      );
    }
    for (const item of value) {
      if (typeof item !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Tracing categories must be strings');
      }
      if (item !== '*' && !available.includes(item)) {
        unsupported(`Tracing category ${item}`);
      }
    }
    return [...value];
  }

  function config(options) {
    object(options);
    const available = JSON.parse(native.traceCategories());
    const legacy = 'categoryFilter' in options || 'traceOptions' in options;
    const allowed = legacy
      ? ['categoryFilter', 'traceOptions']
      : ['included_categories', 'excluded_categories', 'recording_mode'];
    for (const key of Reflect.ownKeys(options)) {
      if (!allowed.includes(key)) {
        unsupported(`Tracing option ${String(key)}`);
      }
    }
    let included = ['*'];
    let excluded = [];
    let mode = options.recording_mode ?? 'record-until-full';
    if (legacy) {
      const filter = options.categoryFilter ?? '*';
      const flags = options.traceOptions ?? 'record-until-full';
      if (typeof filter !== 'string' || typeof flags !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Legacy tracing options must be strings');
      }
      if (filter.length > 1024 || flags.length > 1024) {
        throw error('ERR_OUT_OF_RANGE', 'Tracing options exceed budget');
      }
      included = [];
      for (const entry of filter
        .split(',')
        .map((part) => part.trim())
        .filter(Boolean)) {
        if (entry.startsWith('-')) {
          excluded.push(entry.slice(1));
        } else {
          included.push(entry);
        }
      }
      if (!included.length) {
        included = ['*'];
      }
      const modes = flags
        .split(',')
        .map((part) => part.trim())
        .filter(Boolean);
      if (modes.length > 1) {
        unsupported(`Tracing flags ${flags}`);
      }
      mode = modes[0] ?? 'record-until-full';
    } else {
      included = options.included_categories ?? included;
      excluded = options.excluded_categories ?? excluded;
    }
    if (!['record-until-full', 'record-continuously'].includes(mode)) {
      unsupported(`Tracing recording mode ${mode}`);
    }
    return JSON.stringify({
      schema: 1,
      continuous: mode === 'record-continuously',
      included: categories(included, available),
      excluded: categories(excluded, available)
    });
  }

  return {
    async getCategories() {
      ready();
      return JSON.parse(native.traceCategories());
    },

    async startRecording(options) {
      ready();
      const wire = config(options);
      if (state === 'recording') {
        return;
      }
      if (state !== 'idle') {
        throw error('ERR_FIREFLY_TRACE_BUSY', 'Finish saving the previous recording first');
      }
      native.traceStart(wire);
      state = 'recording';
    },

    async getTraceBufferUsage() {
      ready();
      const value = native.traceUsage();
      return { value, percentage: value * 100 };
    },

    async stopRecording(resultFilePath) {
      ready();
      if (resultFilePath !== undefined && typeof resultFilePath !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Trace output path must be a string');
      }
      if (resultFilePath && (resultFilePath.length > 32768 || resultFilePath.includes('\0'))) {
        throw error('ERR_INVALID_ARG_VALUE', 'Invalid trace output path');
      }
      if (state === 'idle') {
        throw error('ERR_FIREFLY_TRACE_NOT_RECORDING', 'No recording to stop');
      }
      if (state === 'saving') {
        throw error('ERR_FIREFLY_TRACE_BUSY', 'Trace output is already being saved');
      }
      if (state === 'recording') {
        pending = native.traceFinish();
        state = 'pending';
      }
      state = 'saving';
      try {
        if (!resultFilePath && !temporary) {
          temporary = native.traceTemporaryPath();
        }
        const output = resultFilePath ? paths.resolve(resultFilePath) : temporary;
        await files.promises.writeFile(output, pending, {
          encoding: 'utf8',
          mode: 0o600,
          flag: resultFilePath ? 'w' : 'wx'
        });
        pending = undefined;
        temporary = undefined;
        state = 'idle';
        return output;
      } catch (failure) {
        // Keep the same bounded snapshot for stopRecording(newPath) after I/O failure.
        state = 'pending';
        throw failure;
      }
    }
  };
}
