/* Generated from mite/modules/system/native-keymap.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/system/native-keymap.mjs
function createNativeKeymap(native) {
  const listeners = [];
  let watching = false;
  function current() {
    return JSON.parse(native.read());
  }
  return {
    getCurrentKeyboardLayout() {
      return current().layout;
    },
    getKeyMap() {
      return current().map;
    },
    isISOKeyboard() {
      return current().iso;
    },
    onDidChangeKeyboardLayout(callback) {
      if (typeof callback !== "function") {
        throw Object.assign(new TypeError("Keyboard layout listener must be a function"), {
          code: "ERR_INVALID_ARG_TYPE"
        });
      }
      if (listeners.length >= 64) {
        throw Object.assign(new RangeError("Keyboard layout listener limit exceeded"), {
          code: "ERR_MITE_KEYBOARD_LIMIT"
        });
      }
      if (!watching) {
        native.watch(() => {
          for (const listener of [...listeners]) {
            listener();
          }
        });
        watching = true;
      }
      listeners.push(callback);
    }
  };
}
return createNativeKeymap;
})()
