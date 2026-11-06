/** Install portable event classes and authored cancellation policy without LibWeb. */
import { Event, EventTarget, defineEventAttribute, setErrorHandler } from 'event-target-shim';
import createAbortGlobals from '../../mite/runtime/bootstrap/web/abort.mjs';
import createDOMException from '../../mite/runtime/bootstrap/web/dom-exception.mjs';

export default function installWebGlobals() {
  const tick = process.nextTick;

  setErrorHandler((error) =>
    tick(() => {
      throw error;
    })
  );

  const DOMException = createDOMException();
  const hooks = createAbortGlobals({ Event, EventTarget, defineEventAttribute, DOMException });
  for (const [name, value] of Object.entries({
    Event,
    EventTarget,
    DOMException,
    AbortController: hooks.AbortController,
    AbortSignal: hooks.AbortSignal
  })) {
    Object.defineProperty(globalThis, name, { value, writable: true, configurable: true });
  }
  return hooks;
}
