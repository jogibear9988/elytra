/** Main-process appearance policy; native state stays separate from override selection. */
export default function createTheme(native, EventEmitter, error) {
  const nativeTheme = new EventEmitter();
  let source = 'system';
  let baseline = null;
  const fields = [
    'shouldUseHighContrastColors',
    'shouldUseInvertedColorScheme',
    'prefersReducedTransparency',
    'shouldDifferentiateWithoutColor'
  ];

  function requireNativeTheme() {
    if (native.themeAvailable === false) {
      throw error(
        'ERR_FIREFLY_UNSUPPORTED_API',
        'Native theme services are not implemented on this platform'
      );
    }
  }

  function snapshot() {
    requireNativeTheme();
    const system = JSON.parse(native.themeSnapshot());
    const dark = source === 'system' ? system.dark : source === 'dark';
    const value = {
      themeSource: source,
      shouldUseDarkColors: dark,
      shouldUseDarkColorsForSystemIntegratedUI: dark
    };
    for (const key of fields) {
      value[key] = system[key];
    }
    if (!baseline) {
      baseline = value;
    }
    return value;
  }

  Object.defineProperty(nativeTheme, 'themeSource', {
    enumerable: true,

    get() {
      requireNativeTheme();
      return source;
    },

    set(value) {
      requireNativeTheme();
      if (!['system', 'light', 'dark'].includes(value)) {
        throw error('ERR_INVALID_ARG_VALUE', 'themeSource must be system, light or dark');
      }
      if (value === source) {
        return;
      }
      snapshot();
      native.themeSetSource(value);
      source = value;
    }
  });
  for (const key of [
    'shouldUseDarkColors',
    'shouldUseDarkColorsForSystemIntegratedUI',
    ...fields
  ]) {
    Object.defineProperty(nativeTheme, key, {
      enumerable: true,

      get: () => snapshot()[key]
    });
  }

  nativeTheme.on('newListener', (name) => {
    if (name === 'updated') {
      snapshot();
    }
  });

  function changed() {
    if (!baseline) {
      return;
    }
    const next = snapshot();
    const previous = baseline;
    baseline = next; // Reentrant observers see the committed state.
    if (Object.keys(next).some((key) => next[key] !== previous[key])) {
      nativeTheme.emit('updated');
    }
  }

  return { nativeTheme, changed };
}
