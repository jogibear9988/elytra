/** Refuse comparator cases the pinned provider cannot prove against modern Node. */
export function unavailable(feature) {
  throw Object.assign(new Error(`node:assert ${feature} is not implemented`), {
    code: 'ERR_MITE_UNSUPPORTED_API'
  });
}

export function checkDeepValues(actual, expected) {
  if (actual === expected) {
    return;
  }
  const seen = new WeakSet();
  let remaining = 10000;

  function visit(value, depth) {
    if (value === null || typeof value !== 'object' || seen.has(value)) {
      return;
    }
    if (depth > 256 || --remaining < 0) {
      unavailable('deep comparison beyond the supported traversal budget');
    }
    seen.add(value);
    if (Object.hasOwn(value, Symbol.toStringTag)) {
      unavailable('custom comparison tags');
    }
    const tag = Object.prototype.toString.call(value);
    const allowed = [
      '[object Object]',
      '[object Array]',
      '[object Date]',
      '[object RegExp]',
      '[object Error]',
      '[object Map]',
      '[object Set]',
      '[object ArrayBuffer]',
      '[object DataView]',
      '[object Uint8Array]',
      '[object Uint8ClampedArray]',
      '[object Int8Array]',
      '[object Uint16Array]',
      '[object Int16Array]',
      '[object Uint32Array]',
      '[object Int32Array]',
      '[object Number]',
      '[object String]',
      '[object Boolean]',
      '[object Symbol]'
    ];
    if (!allowed.includes(tag)) {
      unavailable(`deep comparison of ${tag}`);
    }
    if (tag === '[object Date]' && Number.isNaN(Date.prototype.getTime.call(value))) {
      unavailable('invalid Date comparison');
    }
    if (tag === '[object RegExp]' && value.lastIndex !== 0) {
      unavailable('stateful RegExp comparison');
    }
    if (tag === '[object Error]' && ('cause' in value || 'errors' in value)) {
      unavailable('Error cause/AggregateError comparison');
    }
    if (tag === '[object Map]') {
      for (const [key, item] of Map.prototype.entries.call(value)) {
        visit(key, depth + 1);
        visit(item, depth + 1);
      }
    }
    if (tag === '[object Set]') {
      for (const item of Set.prototype.values.call(value)) {
        visit(item, depth + 1);
      }
    }
    for (const key of Reflect.ownKeys(value)) {
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      if (!descriptor.enumerable) {
        continue;
      }
      if (!('value' in descriptor)) {
        unavailable('enumerable accessor comparison');
      }
      visit(descriptor.value, depth + 1);
    }
  }

  visit(actual, 0);
  visit(expected, 0);
}
