/** Install portable event classes and authored cancellation policy without LibWeb. */
import {
  Event,
  EventTarget,
  getEventAttributeValue,
  setEventAttributeValue,
  setErrorHandler
} from 'event-target-shim';
import createAbortGlobals from '../../mite/runtime/bootstrap/web/abort.mjs';
import createDOMException from '../../mite/runtime/bootstrap/web/dom-exception.mjs';
import * as streams from 'web-streams-polyfill';
import createWeakRefAdapter, {
  createFinalizationAdapter
} from '../../mite/runtime/bootstrap/web/weak-lifetime.mjs';

export default function installWebGlobals(host) {
  const WeakRef = createWeakRefAdapter(host.keepAlive);
  Object.defineProperty(globalThis, 'FinalizationRegistry', {
    value: createFinalizationAdapter(),
    writable: true,
    configurable: true
  });
  Object.defineProperty(globalThis, 'WeakRef', {
    value: WeakRef,
    writable: true,
    configurable: true
  });
  const tick = process.nextTick;

  setErrorHandler((error) =>
    tick(() => {
      throw error;
    })
  );

  const DOMException = createDOMException();
  const hooks = createAbortGlobals({
    Event,
    EventTarget,
    getEventAttributeValue,
    setEventAttributeValue,
    DOMException
  });
  for (const [name, value] of Object.entries({
    Event,
    EventTarget,
    DOMException,
    AbortController: hooks.AbortController,
    AbortSignal: hooks.AbortSignal,
    ...streams
  })) {
    Object.defineProperty(globalThis, name, { value, writable: true, configurable: true });
  }
  // Keep the module's original constructors alive independently of writable globals.
  // Copies avoid exposing esbuild's internal namespace getters to application code.
  return { abort: hooks, streams: { ...streams } };
}
