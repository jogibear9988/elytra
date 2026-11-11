/* Generated from mite/modules/core/diagnostics-channel.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/diagnostics-channel.mjs
/*!
Copyright Joyent, Inc. and other Node contributors.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
function createDiagnosticsChannel({ process }) {
  const ArrayPrototypeAt = Function.call.bind(Array.prototype.at);
  const ArrayPrototypeIndexOf = Function.call.bind(Array.prototype.indexOf);
  const ArrayPrototypePush = Function.call.bind(Array.prototype.push);
  const ArrayPrototypePushApply = (array, values) => Reflect.apply(Array.prototype.push, array, values);
  const ArrayPrototypeSlice = Function.call.bind(Array.prototype.slice);
  const ArrayPrototypeSplice = Function.call.bind(Array.prototype.splice);
  const ObjectDefineProperty = Object.defineProperty;
  const ObjectGetPrototypeOf = Object.getPrototypeOf;
  const ObjectSetPrototypeOf = Object.setPrototypeOf;
  const PromisePrototypeThen = Function.call.bind(Promise.prototype.then);
  const PromiseReject = Promise.reject.bind(Promise);
  const PromiseResolve = Promise.resolve.bind(Promise);
  const ReflectApply = Reflect.apply;
  const SafeFinalizationRegistry = FinalizationRegistry;
  const SafeMap = Map;
  const SymbolHasInstance = Symbol.hasInstance;
  class ERR_INVALID_ARG_TYPE extends TypeError {
    constructor(name, expected) {
      super(`${name} must be ${expected.join(" or ")}`);
      this.code = "ERR_INVALID_ARG_TYPE";
    }
  }
  function validateFunction(value, name) {
    if (typeof value !== "function") {
      throw new ERR_INVALID_ARG_TYPE(name, ["function"]);
    }
  }
  const triggerUncaughtException = (error) => {
    throw error;
  };
  class WeakReference {
    #weak;
    #strong;
    #count = 0;
    constructor(value) {
      this.#weak = new WeakRef(value);
    }
    get() {
      return this.#strong ?? this.#weak.deref();
    }
    incRef() {
      this.#strong = this.get();
      this.#count++;
    }
    decRef() {
      if (this.#count > 0 && --this.#count === 0) {
        this.#strong = void 0;
      }
    }
  }
  class WeakRefMap extends SafeMap {
    #finalizers = new SafeFinalizationRegistry((key) => {
      if (!this.has(key)) {
        this.delete(key);
      }
    });
    set(key, value) {
      this.#finalizers.register(value, key);
      return super.set(key, new WeakReference(value));
    }
    get(key) {
      return super.get(key)?.get();
    }
    has(key) {
      return !!this.get(key);
    }
    incRef(key) {
      return super.get(key)?.incRef();
    }
    decRef(key) {
      return super.get(key)?.decRef();
    }
  }
  function markActive(channel2) {
    ObjectSetPrototypeOf(channel2, ActiveChannel.prototype);
    channel2._subscribers = [];
    channel2._stores = new SafeMap();
  }
  function maybeMarkInactive(channel2) {
    if (!channel2._subscribers.length && !channel2._stores.size) {
      ObjectSetPrototypeOf(channel2, Channel.prototype);
      channel2._subscribers = void 0;
      channel2._stores = void 0;
    }
  }
  function defaultTransform(data) {
    return data;
  }
  function wrapStoreRun(store, data, next, transform = defaultTransform) {
    return () => {
      let context;
      try {
        context = transform(data);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
        return next();
      }
      return store.run(context, next);
    };
  }
  class ActiveChannel {
    subscribe(subscription) {
      validateFunction(subscription, "subscription");
      this._subscribers = ArrayPrototypeSlice(this._subscribers);
      ArrayPrototypePush(this._subscribers, subscription);
      channels.incRef(this.name);
    }
    unsubscribe(subscription) {
      const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
      if (index === -1) {
        return false;
      }
      const before = ArrayPrototypeSlice(this._subscribers, 0, index);
      const after = ArrayPrototypeSlice(this._subscribers, index + 1);
      this._subscribers = before;
      ArrayPrototypePushApply(this._subscribers, after);
      channels.decRef(this.name);
      maybeMarkInactive(this);
      return true;
    }
    bindStore(store, transform) {
      const replacing = this._stores.has(store);
      if (!replacing) {
        channels.incRef(this.name);
      }
      this._stores.set(store, transform);
    }
    unbindStore(store) {
      if (!this._stores.has(store)) {
        return false;
      }
      this._stores.delete(store);
      channels.decRef(this.name);
      maybeMarkInactive(this);
      return true;
    }
    get hasSubscribers() {
      return true;
    }
    publish(data) {
      const subscribers = this._subscribers;
      for (let i = 0; i < (subscribers?.length || 0); i++) {
        try {
          const onMessage = subscribers[i];
          onMessage(data, this.name);
        } catch (err) {
          process.nextTick(() => {
            triggerUncaughtException(err, false);
          });
        }
      }
    }
    runStores(data, fn, thisArg, ...args) {
      let run = () => {
        this.publish(data);
        return ReflectApply(fn, thisArg, args);
      };
      for (const entry of this._stores.entries()) {
        const store = entry[0];
        const transform = entry[1];
        run = wrapStoreRun(store, data, run, transform);
      }
      return run();
    }
  }
  class Channel {
    constructor(name) {
      this._subscribers = void 0;
      this._stores = void 0;
      this.name = name;
      channels.set(name, this);
    }
    static [SymbolHasInstance](instance) {
      const prototype = ObjectGetPrototypeOf(instance);
      return prototype === Channel.prototype || prototype === ActiveChannel.prototype;
    }
    subscribe(subscription) {
      markActive(this);
      this.subscribe(subscription);
    }
    unsubscribe() {
      return false;
    }
    bindStore(store, transform) {
      markActive(this);
      this.bindStore(store, transform);
    }
    unbindStore() {
      return false;
    }
    get hasSubscribers() {
      return false;
    }
    publish() {
    }
    runStores(data, fn, thisArg, ...args) {
      return ReflectApply(fn, thisArg, args);
    }
  }
  const channels = new WeakRefMap();
  function channel(name) {
    const channel2 = channels.get(name);
    if (channel2) {
      return channel2;
    }
    if (typeof name !== "string" && typeof name !== "symbol") {
      throw new ERR_INVALID_ARG_TYPE("channel", ["string", "symbol"], name);
    }
    return new Channel(name);
  }
  function subscribe(name, subscription) {
    return channel(name).subscribe(subscription);
  }
  function unsubscribe(name, subscription) {
    return channel(name).unsubscribe(subscription);
  }
  function hasSubscribers(name) {
    const channel2 = channels.get(name);
    if (!channel2) {
      return false;
    }
    return channel2.hasSubscribers;
  }
  const traceEvents = ["start", "end", "asyncStart", "asyncEnd", "error"];
  function assertChannel(value, name) {
    if (!(value instanceof Channel)) {
      throw new ERR_INVALID_ARG_TYPE(name, ["Channel"], value);
    }
  }
  function tracingChannelFrom(nameOrChannels, name) {
    if (typeof nameOrChannels === "string") {
      return channel(`tracing:${nameOrChannels}:${name}`);
    }
    if (typeof nameOrChannels === "object" && nameOrChannels !== null) {
      const channel2 = nameOrChannels[name];
      assertChannel(channel2, `nameOrChannels.${name}`);
      return channel2;
    }
    throw new ERR_INVALID_ARG_TYPE(
      "nameOrChannels",
      ["string", "object", "TracingChannel"],
      nameOrChannels
    );
  }
  class TracingChannel {
    constructor(nameOrChannels) {
      for (let i = 0; i < traceEvents.length; ++i) {
        const eventName = traceEvents[i];
        ObjectDefineProperty(this, eventName, {
          __proto__: null,
          value: tracingChannelFrom(nameOrChannels, eventName)
        });
      }
    }
    get hasSubscribers() {
      return this.start?.hasSubscribers || this.end?.hasSubscribers || this.asyncStart?.hasSubscribers || this.asyncEnd?.hasSubscribers || this.error?.hasSubscribers;
    }
    subscribe(handlers) {
      for (let i = 0; i < traceEvents.length; ++i) {
        const name = traceEvents[i];
        if (!handlers[name]) {
          continue;
        }
        this[name]?.subscribe(handlers[name]);
      }
    }
    unsubscribe(handlers) {
      let done = true;
      for (let i = 0; i < traceEvents.length; ++i) {
        const name = traceEvents[i];
        if (!handlers[name]) {
          continue;
        }
        if (!this[name]?.unsubscribe(handlers[name])) {
          done = false;
        }
      }
      return done;
    }
    traceSync(fn, context = {}, thisArg, ...args) {
      if (!this.hasSubscribers) {
        return ReflectApply(fn, thisArg, args);
      }
      const { start, end, error } = this;
      return start.runStores(context, () => {
        try {
          const result = ReflectApply(fn, thisArg, args);
          context.result = result;
          return result;
        } catch (err) {
          context.error = err;
          error.publish(context);
          throw err;
        } finally {
          end.publish(context);
        }
      });
    }
    tracePromise(fn, context = {}, thisArg, ...args) {
      if (!this.hasSubscribers) {
        return ReflectApply(fn, thisArg, args);
      }
      const { start, end, asyncStart, asyncEnd, error } = this;
      function reject(err) {
        context.error = err;
        error.publish(context);
        asyncStart.publish(context);
        asyncEnd.publish(context);
        return PromiseReject(err);
      }
      function resolve(result) {
        context.result = result;
        asyncStart.publish(context);
        asyncEnd.publish(context);
        return result;
      }
      return start.runStores(context, () => {
        try {
          let promise = ReflectApply(fn, thisArg, args);
          if (!(promise instanceof Promise)) {
            promise = PromiseResolve(promise);
          }
          return PromisePrototypeThen(promise, resolve, reject);
        } catch (err) {
          context.error = err;
          error.publish(context);
          throw err;
        } finally {
          end.publish(context);
        }
      });
    }
    traceCallback(fn, position = -1, context = {}, thisArg, ...args) {
      if (!this.hasSubscribers) {
        return ReflectApply(fn, thisArg, args);
      }
      const { start, end, asyncStart, asyncEnd, error } = this;
      function wrappedCallback(err, res) {
        if (err) {
          context.error = err;
          error.publish(context);
        } else {
          context.result = res;
        }
        asyncStart.runStores(context, () => {
          try {
            return ReflectApply(callback, this, arguments);
          } finally {
            asyncEnd.publish(context);
          }
        });
      }
      const callback = ArrayPrototypeAt(args, position);
      validateFunction(callback, "callback");
      ArrayPrototypeSplice(args, position, 1, wrappedCallback);
      return start.runStores(context, () => {
        try {
          return ReflectApply(fn, thisArg, args);
        } catch (err) {
          context.error = err;
          error.publish(context);
          throw err;
        } finally {
          end.publish(context);
        }
      });
    }
  }
  function tracingChannel(nameOrChannels) {
    return new TracingChannel(nameOrChannels);
  }
  return { channel, hasSubscribers, subscribe, tracingChannel, unsubscribe, Channel };
}
return createDiagnosticsChannel;
})()
