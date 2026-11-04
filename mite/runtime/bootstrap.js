(() => {
    'use strict';
    const schedule = globalThis.__miteScheduleTimer;
    const cancel = globalThis.__miteCancelTimer;
    const setRef = globalThis.__miteRefTimer;
    delete globalThis.__miteScheduleTimer;
    delete globalThis.__miteCancelTimer;
    delete globalThis.__miteRefTimer;

    const id = Symbol('timerId');
    const referenced = Symbol('referenced');
    class Timeout {
        constructor(callback, delay, repeat, args) {
            if (typeof callback !== 'function') {
                const error = new TypeError('The callback argument must be of type function');
                error.code = 'ERR_INVALID_ARG_TYPE';
                throw error;
            }
            delay = delay === undefined ? 1 : +delay;
            if (!(delay >= 1 && delay <= 2147483647)) delay = 1;
            this[referenced] = true;
            this[id] = schedule(callback, Math.trunc(delay), repeat, this, ...args);
        }
        ref() { this[referenced] = true; setRef(this[id], true); return this; }
        unref() { this[referenced] = false; setRef(this[id], false); return this; }
        hasRef() { return this[referenced]; }
        close() { cancel(this[id]); return this; }
        [Symbol.toPrimitive]() { return this[id]; }
        [Symbol.dispose]() { cancel(this[id]); }
    }
    globalThis.setTimeout = function setTimeout(callback, delay, ...args) {
        return new Timeout(callback, delay, false, args);
    };
    globalThis.setInterval = function setInterval(callback, delay, ...args) {
        return new Timeout(callback, delay, true, args);
    };
    globalThis.clearTimeout = function clearTimeout(timer) {
        if (timer instanceof Timeout) cancel(timer[id]);
        else if (typeof timer === 'number') cancel(timer);
        else if (typeof timer === 'string' && String(+timer) === timer) cancel(+timer);
    };
    const clearTimer = globalThis.clearTimeout;
    globalThis.clearInterval = function clearInterval(timer) {
        clearTimer(timer);
    };
})();
