/** Numeric browser zoom policy, bounded by the pinned native Ladybird view. */
export default function createZoomPolicy() {
  function number(value) {
    if (typeof value !== 'number') {
      throw Object.assign(new TypeError('Zoom requires a number'), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
    if (!Number.isFinite(value)) {
      throw Object.assign(new RangeError('Zoom requires a finite number'), {
        code: 'ERR_OUT_OF_RANGE'
      });
    }
    return value;
  }

  function factor(value) {
    number(value);
    if (value < 0.3 || value > 5) {
      throw Object.assign(new RangeError('Zoom factor must be within 0.3 and 5'), {
        code: 'ERR_OUT_OF_RANGE'
      });
    }
    return value;
  }

  return {
    factor,

    level(value) {
      return factor(1.2 ** number(value));
    }
  };
}
