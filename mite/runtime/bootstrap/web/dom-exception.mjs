/** Error-shaped Web exception values used by cancellation, independent of LibWeb. */
export default function createDOMException() {
  const states = new WeakMap();
  const legacy = [
    ['IndexSizeError', 'INDEX_SIZE_ERR'],
    ['DOMStringSizeError', 'DOMSTRING_SIZE_ERR'],
    ['HierarchyRequestError', 'HIERARCHY_REQUEST_ERR'],
    ['WrongDocumentError', 'WRONG_DOCUMENT_ERR'],
    ['InvalidCharacterError', 'INVALID_CHARACTER_ERR'],
    ['NoDataAllowedError', 'NO_DATA_ALLOWED_ERR'],
    ['NoModificationAllowedError', 'NO_MODIFICATION_ALLOWED_ERR'],
    ['NotFoundError', 'NOT_FOUND_ERR'],
    ['NotSupportedError', 'NOT_SUPPORTED_ERR'],
    ['InUseAttributeError', 'INUSE_ATTRIBUTE_ERR'],
    ['InvalidStateError', 'INVALID_STATE_ERR'],
    ['SyntaxError', 'SYNTAX_ERR'],
    ['InvalidModificationError', 'INVALID_MODIFICATION_ERR'],
    ['NamespaceError', 'NAMESPACE_ERR'],
    ['InvalidAccessError', 'INVALID_ACCESS_ERR'],
    ['ValidationError', 'VALIDATION_ERR'],
    ['TypeMismatchError', 'TYPE_MISMATCH_ERR'],
    ['SecurityError', 'SECURITY_ERR'],
    ['NetworkError', 'NETWORK_ERR'],
    ['AbortError', 'ABORT_ERR'],
    ['URLMismatchError', 'URL_MISMATCH_ERR'],
    ['QuotaExceededError', 'QUOTA_EXCEEDED_ERR'],
    ['TimeoutError', 'TIMEOUT_ERR'],
    ['InvalidNodeTypeError', 'INVALID_NODE_TYPE_ERR'],
    ['DataCloneError', 'DATA_CLONE_ERR']
  ];
  const codes = new Map(legacy.map(([name], index) => [name, index + 1]));
  // Constants survive for compatibility, but these obsolete exception names no
  // longer map to numeric codes in the current Web/Node exception table.
  for (const name of ['DOMStringSizeError', 'NoDataAllowedError', 'ValidationError']) {
    codes.delete(name);
  }

  function state(value) {
    const found = states.get(value);
    if (!found) {
      const error = new TypeError('Value must be a DOMException');
      error.code = 'ERR_INVALID_THIS';
      throw error;
    }
    return found;
  }

  class DOMException extends Error {
    constructor(message = '', name = 'Error') {
      message = `${message}`;
      name = `${name}`;
      super(message);
      delete this.message;
      states.set(this, { message, name });
    }

    get message() {
      return state(this).message;
    }

    get name() {
      return state(this).name;
    }

    get code() {
      return codes.get(state(this).name) ?? 0;
    }
  }
  Object.defineProperty(DOMException.prototype, Symbol.toStringTag, {
    value: 'DOMException',
    configurable: true
  });

  legacy.forEach(([, constant], index) => {
    for (const target of [DOMException, DOMException.prototype]) {
      Object.defineProperty(target, constant, { value: index + 1, enumerable: true });
    }
  });

  return DOMException;
}
