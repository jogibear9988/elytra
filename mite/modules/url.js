(function (host) {
    'use strict';
    const native = host.native;
    const urls = new WeakMap();
    const params = new WeakMap();
    function fail(code, message) { const error = new TypeError(message); error.code = code; throw error; }
    function usv(value) { return `${value}`.toWellFormed(); }
    function state(map, object) { const value = map.get(object); if (!value) { if (map === urls) throw new TypeError('Invalid URL receiver'); fail('ERR_INVALID_THIS', 'Invalid receiver'); } return value; }
    function required(count, actual) { if (actual < count) fail('ERR_MISSING_ARGS', 'Missing required argument'); }
    function invalid(input, base) {
        const error = new TypeError('Invalid URL'); error.code = 'ERR_INVALID_URL'; error.input = input;
        if (base !== undefined) error.base = base;
        throw error;
    }
    function updateParams(value) {
        if (value.url) value.url.record = native.update(value.url.record.href, 'search', native.encodeForm(value.list));
    }
    class URLSearchParams {
        constructor(init = '') {
            const value = { list: [], url: null };
            params.set(this, value);
            if (init == null) return;
            if (typeof init === 'object' || typeof init === 'function') {
                const iterator = init[Symbol.iterator];
                if (iterator !== undefined && iterator !== null) {
                    if (typeof iterator !== 'function') fail('ERR_ARG_NOT_ITERABLE', 'Query pairs must be iterable');
                    for (const pair of init) {
                        if (pair === null || (typeof pair !== 'object' && typeof pair !== 'function') || typeof pair[Symbol.iterator] !== 'function')
                            fail('ERR_INVALID_TUPLE', 'Each query pair must be iterable with two items');
                        const items = Array.from(pair);
                        if (items.length !== 2) fail('ERR_INVALID_TUPLE', 'Each query pair must contain exactly two items');
                        value.list.push([usv(items[0]), usv(items[1])]);
                    }
                } else {
                    const names = new Map();
                    for (const name of Reflect.ownKeys(init)) {
                        if (!Object.getOwnPropertyDescriptor(init, name).enumerable) continue;
                        const key = usv(name), item = usv(init[name]);
                        if (names.has(key)) value.list[names.get(key)][1] = item;
                        else { names.set(key, value.list.length); value.list.push([key, item]); }
                    }
                }
            } else {
                const text = usv(init); value.list = native.decodeForm(text.startsWith('?') ? text.slice(1) : text);
            }
        }
        get size() { return state(params, this).list.length; }
        append(name, value) { const s = state(params, this); required(2, arguments.length); s.list.push([usv(name), usv(value)]); updateParams(s); }
        delete(name, value) {
            const s = state(params, this); required(1, arguments.length); name = usv(name);
            if (value !== undefined) value = usv(value);
            s.list = s.list.filter(pair => pair[0] !== name || (value !== undefined && pair[1] !== value)); updateParams(s);
        }
        get(name) { const s = state(params, this); required(1, arguments.length); name = usv(name); return s.list.find(pair => pair[0] === name)?.[1] ?? null; }
        getAll(name) { const s = state(params, this); required(1, arguments.length); name = usv(name); return s.list.filter(pair => pair[0] === name).map(pair => pair[1]); }
        has(name, value) { const s = state(params, this); required(1, arguments.length); name = usv(name); if (value !== undefined) value = usv(value); return s.list.some(pair => pair[0] === name && (value === undefined || pair[1] === value)); }
        set(name, value) {
            const s = state(params, this); required(2, arguments.length); name = usv(name); value = usv(value);
            let found = false;
            s.list = s.list.filter(pair => { if (pair[0] !== name) return true; if (found) return false; pair[1] = value; found = true; return true; });
            if (!found) s.list.push([name, value]); updateParams(s);
        }
        sort() { const s = state(params, this); s.list.sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0); updateParams(s); }
        entries() { return iterator(state(params, this), 'entries'); }
        keys() { return iterator(state(params, this), 'keys'); }
        values() { return iterator(state(params, this), 'values'); }
        forEach(callback, thisArg) {
            const s = state(params, this);
            if (typeof callback !== 'function') fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
            for (let i = 0; i < s.list.length; ++i) callback.call(thisArg, s.list[i][1], s.list[i][0], this);
        }
        toString() { return native.encodeForm(state(params, this).list); }
    }
    const iterators = new WeakMap();
    const iteratorPrototype = Object.create(Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
    Object.defineProperty(iteratorPrototype, Symbol.toStringTag, { value: 'URLSearchParams Iterator', configurable: true });
    Object.defineProperty(iteratorPrototype, 'next', { enumerable: true, configurable: true, writable: true,
        value: function next() {
            const s = state(iterators, this);
            if (s.index >= s.source.list.length) return {value:undefined, done:true};
            const pair = s.source.list[s.index++];
            return {value:s.kind === 'keys' ? pair[0] : s.kind === 'values' ? pair[1] : pair.slice(), done:false};
        }
    });
    function iterator(source, kind) { const value=Object.create(iteratorPrototype); iterators.set(value,{source,kind,index:0}); return value; }
    URLSearchParams.prototype[Symbol.iterator] = URLSearchParams.prototype.entries;
    Object.defineProperty(URLSearchParams.prototype, Symbol.toStringTag, { value: 'URLSearchParams', configurable: true });
    class URL {
        constructor(input, base) {
            required(1, arguments.length); input = usv(input); if (base !== undefined) base = usv(base);
            const record = native.parse(input, base); if (!record) invalid(input, base);
            const searchParams = new URLSearchParams(record.search);
            const value = { record, searchParams }; urls.set(this, value);
            Object.assign(params.get(searchParams), {url:value, owner:this});
        }
        static canParse(input, base) { required(1, arguments.length); input = usv(input); if (base !== undefined) base = usv(base); return native.parse(input, base) !== null; }
        static parse(input, base) { required(1, arguments.length); input = usv(input); if (base !== undefined) base = usv(base); const result = native.parse(input, base); return result ? new URL(result.href) : null; }
        get searchParams() { return state(urls, this).searchParams; }
        get origin() { return state(urls, this).record.origin; }
        toString() { const s=urls.get(this); if (!s) throw new TypeError('Invalid URL receiver'); return s.record.href; }
        toJSON() { return state(urls, this).record.href; }
    }
    for (const name of ['href','protocol','username','password','host','hostname','port','pathname','search','hash']) {
        Object.defineProperty(URL.prototype, name, { enumerable: true, configurable: true,
            get() { return state(urls, this).record[name]; },
            set(input) {
                const s = state(urls, this); input = usv(input);
                const value = name === 'href' ? native.parse(input) : native.update(s.record.href, name, input);
                if (!value) invalid(input);
                s.record = value;
                if (name === 'href' || name === 'search') params.get(s.searchParams).list = native.decodeForm(value.search.slice(1));
            }
        });
    }
    Object.defineProperty(URL.prototype, Symbol.toStringTag, { value: 'URL', configurable: true });
    for (const prototype of [URL.prototype, URLSearchParams.prototype]) {
        for (const name of Object.getOwnPropertyNames(prototype)) {
            if (name !== 'constructor') Object.defineProperty(prototype, name, {...Object.getOwnPropertyDescriptor(prototype, name), enumerable:true});
        }
    }
    return {URL, URLSearchParams};
})
