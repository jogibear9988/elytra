/* Generated from mite/modules/network/dns.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/network/dns.mjs
function createDnsModule(host) {
  const native = host.native;
  const nextTick = host.process.nextTick;
  const domainToASCII = host.domainToASCII;
  const ADDRCONFIG = native.ADDRCONFIG;
  const V4MAPPED = native.V4MAPPED;
  const ALL = native.ALL;
  let defaultOrder = "verbatim";
  function createError(Type, code, message) {
    const result = new Type(message);
    result.code = code;
    return result;
  }
  function validateResultOrder(value) {
    if (!["verbatim", "ipv4first", "ipv6first"].includes(value)) {
      throw createError(TypeError, "ERR_INVALID_ARG_VALUE", "Invalid DNS result order");
    }
    return value;
  }
  function normalizeLookupOptions(value) {
    if (value == null) {
      value = {};
    }
    if (typeof value === "number") {
      value = { family: value };
    }
    if (typeof value !== "object") {
      throw createError(TypeError, "ERR_INVALID_ARG_TYPE", "options must be a number or object");
    }
    let family = value.family ?? 0;
    if (family === "IPv4") {
      family = 4;
    }
    if (family === "IPv6") {
      family = 6;
    }
    if (![0, 4, 6].includes(family)) {
      throw createError(TypeError, "ERR_INVALID_ARG_VALUE", "Invalid address family");
    }
    let hints = value.hints ?? 0;
    if (typeof hints !== "number") {
      throw createError(TypeError, "ERR_INVALID_ARG_TYPE", "hints must be a number");
    }
    hints = hints | 0;
    if (hints & ~(ADDRCONFIG | V4MAPPED | ALL)) {
      throw createError(TypeError, "ERR_INVALID_ARG_VALUE", "Invalid getaddrinfo hints");
    }
    for (const key of ["all", "verbatim"]) {
      if (value[key] != null && typeof value[key] !== "boolean") {
        throw createError(TypeError, "ERR_INVALID_ARG_TYPE", `${key} must be a boolean`);
      }
    }
    let resultOrder = defaultOrder;
    if (value.verbatim != null) {
      resultOrder = value.verbatim ? "verbatim" : "ipv4first";
    }
    if (value.order != null) {
      resultOrder = validateResultOrder(value.order);
    }
    return { family, hints, all: value.all ?? false, order: resultOrder };
  }
  function createLookupError(status, hostname) {
    const { code } = native.error(status);
    const lookupError = createError(Error, code, `getaddrinfo ${code} ${hostname}`);
    lookupError.errno = status;
    lookupError.syscall = "getaddrinfo";
    lookupError.hostname = hostname;
    return lookupError;
  }
  function lookup(hostname, opts, callback) {
    if (typeof opts === "function") {
      callback = opts;
      opts = void 0;
    }
    if (typeof callback !== "function") {
      throw createError(TypeError, "ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    if (hostname && typeof hostname !== "string") {
      throw createError(TypeError, "ERR_INVALID_ARG_TYPE", "hostname must be a string");
    }
    const config = normalizeLookupOptions(opts);
    if (!hostname) {
      nextTick(
        () => config.all ? callback(null, []) : callback(null, null, config.family === 6 ? 6 : 4)
      );
      return {};
    }
    const zone = hostname.indexOf("%");
    const family = zone !== -1 && !/^[0-9A-Za-z.:-]+$/.test(hostname.slice(zone + 1)) ? 0 : native.isIP(hostname);
    if (family) {
      nextTick(
        () => config.all ? callback(null, [{ address: hostname, family }]) : callback(null, hostname, family)
      );
      return {};
    }
    let normalized = hostname.includes("\0") ? "" : domainToASCII(hostname);
    const complete = (status2, values) => {
      if (status2) {
        callback(createLookupError(status2, hostname));
        return;
      }
      if (config.order !== "verbatim") {
        const first = config.order === "ipv4first" ? 4 : 6;
        values = values.filter((value) => value.family === first).concat(values.filter((value) => value.family !== first));
      }
      if (config.all) {
        callback(null, values);
      } else {
        callback(null, values[0].address, values[0].family);
      }
    };
    const status = native.lookup(normalized || "\0", config.family, config.hints, complete);
    if (status) {
      nextTick(() => complete(status, []));
    }
    return {};
  }
  function setDefaultResultOrder(value) {
    defaultOrder = validateResultOrder(value);
  }
  function getDefaultResultOrder() {
    return defaultOrder;
  }
  const promises = {
    lookup: async (hostname, opts) => {
      if (opts && typeof opts === "object" && typeof opts.family === "string") {
        throw createError(TypeError, "ERR_INVALID_ARG_VALUE", "Invalid address family");
      }
      return new Promise(
        (resolve, reject) => lookup(hostname, opts, (e, address, family) => {
          if (e) {
            reject(e);
          } else {
            resolve(Array.isArray(address) ? address : { address, family });
          }
        })
      );
    },
    setDefaultResultOrder,
    getDefaultResultOrder
  };
  return {
    lookup,
    promises,
    setDefaultResultOrder,
    getDefaultResultOrder,
    ADDRCONFIG,
    V4MAPPED,
    ALL
  };
}
return createDnsModule;
})()
