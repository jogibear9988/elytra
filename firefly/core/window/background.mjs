/** Electron color spelling with an explicitly opaque native canvas contract. */
export default function createBackground(native, error, unsupported) {
  const white = 0xffffff;

  function packed(value) {
    if (!Number.isInteger(value) || value < 0 || value > white) {
      throw error('ERR_FIREFLY_COLOR_DATA', 'Invalid native background color');
    }
    return value;
  }

  function opaque(red, green, blue, alpha = 1) {
    if (alpha !== 1) {
      unsupported('Non-opaque window/view backgrounds');
    }
    return (Math.round(red) << 16) | (Math.round(green) << 8) | Math.round(blue);
  }

  function parse(value) {
    if (typeof value !== 'string') {
      throw Object.assign(new TypeError('Background color must be a string'), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
    // Electron uses white for invalid color strings. Bound parsing without ever
    // retaining application-sized input or passing it to a renderer.
    if (value.length > 256) {
      return white;
    }
    const hex = /^#([\da-f]{3}|[\da-f]{4}|[\da-f]{6}|[\da-f]{8})$/i.exec(value);
    if (hex) {
      let digits = hex[1];
      if (digits.length <= 4) {
        digits = [...digits].map((digit) => digit + digit).join('');
      }
      const alpha = digits.length === 8 ? Number.parseInt(digits.slice(0, 2), 16) / 255 : 1;
      const rgb = digits.slice(-6);
      return opaque(
        Number.parseInt(rgb.slice(0, 2), 16),
        Number.parseInt(rgb.slice(2, 4), 16),
        Number.parseInt(rgb.slice(4, 6), 16),
        alpha
      );
    }
    const fn = /^(rgb|rgba|hsl|hsla)\((.*)\)$/.exec(value);
    if (fn) {
      const components = fn[2].split(',').map((part) => part.trim());
      const alphaPresent = fn[1].endsWith('a');
      if (components.length !== (alphaPresent ? 4 : 3)) {
        return white;
      }
      const decimal = /^[-+]?(?:\d+(?:\.\d*)?|\.\d+)$/;
      let alpha = 1;
      if (alphaPresent) {
        if (!decimal.test(components[3])) {
          return white;
        }
        alpha = Number(components[3]);
        if (!Number.isFinite(alpha) || alpha < 0 || alpha > 1) {
          return white;
        }
      }
      if (fn[1].startsWith('rgb')) {
        if (components.slice(0, 3).some((part) => !/^\d+$/.test(part))) {
          return white;
        }
        const [r, g, b] = components.slice(0, 3).map(Number);
        return [r, g, b].some((channel) => channel > 255) ? white : opaque(r, g, b, alpha);
      }
      if (
        !decimal.test(components[0]) ||
        components
          .slice(1, 3)
          .some((part) => !part.endsWith('%') || !decimal.test(part.slice(0, -1)))
      ) {
        return white;
      }
      const hue = Number(components[0]);
      const saturation = Number(components[1].slice(0, -1)) / 100;
      const light = Number(components[2].slice(0, -1)) / 100;
      if (!Number.isFinite(hue) || saturation < 0 || saturation > 1 || light < 0 || light > 1) {
        return white;
      }
      const h = (((hue % 360) + 360) % 360) / 60;
      const c = (1 - Math.abs(2 * light - 1)) * saturation;
      const x = c * (1 - Math.abs((h % 2) - 1));
      const channels =
        h < 1
          ? [c, x, 0]
          : h < 2
            ? [x, c, 0]
            : h < 3
              ? [0, c, x]
              : h < 4
                ? [0, x, c]
                : h < 5
                  ? [x, 0, c]
                  : [c, 0, x];
      const offset = light - c / 2;
      return opaque(...channels.map((channel) => (channel + offset) * 255), alpha);
    }
    if (value !== 'transparent' && /^[a-z]{1,32}$/.test(value)) {
      if (typeof native.lookupColor !== 'function') {
        unsupported('Native named background colors');
      }
      const result = native.lookupColor(value);
      return result === null ? white : packed(result);
    }
    return white;
  }

  return {
    parse,

    get(id) {
      if (typeof native.contentsBackground !== 'function') {
        unsupported('Native contents background');
      }
      return (
        '#' + packed(native.contentsBackground(id)).toString(16).padStart(6, '0').toUpperCase()
      );
    },

    set(id, value) {
      const color = parse(value);
      if (typeof native.setContentsBackground !== 'function') {
        unsupported('Native contents background');
      }
      native.setContentsBackground(id, color);
    }
  };
}
