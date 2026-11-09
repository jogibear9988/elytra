/** Shared DIP geometry policy; native results remain authoritative after OS constraints. */
import createConstraints from './constraints.mjs';

export default function createWindowGeometry(
  native,
  EventEmitter,
  alive,
  dimension,
  error,
  unsupported
) {
  const constraints = createConstraints(native, error, unsupported);

  function coordinate(value, name) {
    if (
      typeof value !== 'number' ||
      !Number.isInteger(value) ||
      value < -2147483648 ||
      value > 2147483647
    ) {
      throw error('ERR_INVALID_ARG_TYPE', `${name} must be a signed 32-bit integer`);
    }
    return value;
  }

  function get(window, content) {
    alive(window);
    return JSON.parse(native.windowBounds(window.id, content));
  }

  function set(window, value, content, animate) {
    alive(window);
    if (animate) {
      unsupported('animated window geometry');
    }
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'bounds must be a rectangle object');
    }
    const updates = {};
    if (content && ['x', 'y', 'width', 'height'].some((key) => !Object.hasOwn(value, key))) {
      throw error('ERR_INVALID_ARG_VALUE', 'Content bounds require a complete rectangle');
    }
    for (const key of Object.keys(value)) {
      if (!['x', 'y', 'width', 'height'].includes(key)) {
        unsupported(`window bounds ${key}`);
      }
      updates[key] = ['width', 'height'].includes(key)
        ? dimension(value[key], key)
        : coordinate(value[key], key);
    }
    const rect = { ...get(window, content), ...updates };
    if (rect.x + rect.width - 1 > 2147483647 || rect.y + rect.height - 1 > 2147483647) {
      throw error('ERR_OUT_OF_RANGE', 'Window rectangle exceeds signed 32-bit coordinates');
    }
    native.setWindowBounds(window.id, rect.x, rect.y, rect.width, rect.height, content);
  }

  // Keep geometry methods together while preserving EventEmitter ownership in BrowserWindow.
  class WindowGeometry extends EventEmitter {
    getMinimumSize() {
      alive(this);
      return constraints.get(this.id, true);
    }

    getMaximumSize() {
      alive(this);
      return constraints.get(this.id, false);
    }

    setMinimumSize(width, height) {
      alive(this);
      constraints.set(this.id, width, height, true);
    }

    setMaximumSize(width, height) {
      alive(this);
      constraints.set(this.id, width, height, false);
    }

    getBounds() {
      return get(this, false);
    }

    getContentBounds() {
      return get(this, true);
    }

    setBounds(bounds, animate = false) {
      set(this, bounds, false, animate);
    }

    setContentBounds(bounds, animate = false) {
      set(this, bounds, true, animate);
    }

    getSize() {
      const { width, height } = this.getBounds();
      return [width, height];
    }

    getContentSize() {
      const { width, height } = this.getContentBounds();
      return [width, height];
    }

    setSize(width, height, animate = false) {
      this.setBounds({ width, height }, animate);
    }

    setContentSize(width, height, animate = false) {
      this.setContentBounds({ ...this.getContentBounds(), width, height }, animate);
    }

    getPosition() {
      const { x, y } = this.getBounds();
      return [x, y];
    }

    setPosition(x, y, animate = false) {
      this.setBounds({ x, y }, animate);
    }

    center() {
      alive(this);
      native.centerWindow(this.id);
    }
  }

  return { WindowGeometry, coordinate, initialConstraints: constraints.initial };
}
