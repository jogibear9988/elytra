/** Legacy constructor inheritance used by real CommonJS packages such as yazl. */
export default function inherits(ctor, superCtor) {
  function invalid(name) {
    throw Object.assign(new TypeError(`${name} is not a supported inheritance target`), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }

  // Preserve Node's deliberately permissive object/prototype contract. This is not
  // class extends: existing methods and the constructor's own static parent survive.
  if (ctor === null || ctor === undefined) {
    invalid('ctor');
  }
  if (superCtor === null || superCtor === undefined) {
    invalid('superCtor');
  }
  if (superCtor.prototype === undefined) {
    invalid('superCtor.prototype');
  }
  Object.defineProperty(ctor, 'super_', {
    value: superCtor,
    writable: true,
    configurable: true
  });
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
}
