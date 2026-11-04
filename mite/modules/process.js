(function (host, process) {
    'use strict';
    const own = Function.call.bind(Object.prototype.hasOwnProperty);
    function invalid(code, message, Type = TypeError) {
        const error = new Type(message);
        error.code = code;
        return error;
    }
    process.env = new Proxy({}, {
        get(target, key, receiver) {
            if (typeof key !== 'string') return Reflect.get(target, key, receiver);
            const value = host.envGet(key);
            return value === undefined ? Reflect.get(target, key, receiver) : value;
        },
        set(target, key, value) {
            if (typeof key !== 'string') throw new TypeError('Environment keys must be strings');
            host.envSet(key, `${value}`);
            return true;
        },
        has(target, key) { return typeof key === 'string' && host.envGet(key) !== undefined; },
        deleteProperty(target, key) {
            if (typeof key === 'string') host.envDelete(key);
            return true;
        },
        ownKeys() { return host.envKeys(); },
        getOwnPropertyDescriptor(target, key) {
            if (typeof key !== 'string') return undefined;
            const value = host.envGet(key);
            if (value === undefined) return undefined;
            return { value, writable: true, enumerable: true, configurable: true };
        },
        defineProperty(target, key, descriptor) {
            if (typeof key !== 'string' || !own(descriptor, 'value') || !descriptor.writable || !descriptor.enumerable
                || !descriptor.configurable || own(descriptor, 'get') || own(descriptor, 'set'))
                throw invalid('ERR_INVALID_OBJECT_DEFINE_PROPERTY', 'Environment properties must be configurable writable enumerable data properties');
            host.envSet(key, `${descriptor.value}`);
            return true;
        },
        preventExtensions() { return false; },
    });
    let exitCode;
    Object.defineProperty(process, 'exitCode', {
        configurable: true,
        enumerable: true,
        get() { return exitCode; },
        set(value) {
            if (value === undefined || value === null) { exitCode = undefined; return; }
            if (typeof value === 'string' && value !== '' && !Number.isNaN(+value)) value = +value;
            if (typeof value !== 'number') throw invalid('ERR_INVALID_ARG_TYPE', 'Exit code must be a number or integer string');
            if (!Number.isInteger(value)) throw invalid('ERR_OUT_OF_RANGE', 'Exit code must be an integer', RangeError);
            exitCode = value | 0;
        },
    });
    process.exit = function exit(code) {
        if (code !== undefined) process.exitCode = code;
        host.exit(process.exitCode === undefined ? 0 : process.exitCode);
    };
    return process;
})
