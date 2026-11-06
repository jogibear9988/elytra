/** The document has only a frozen preload API; privileged work remains in Mite. */
try {
  if (
    typeof process !== 'undefined' ||
    typeof require !== 'undefined' ||
    !Object.isFrozen(desktop) ||
    typeof desktop.roundtrip !== 'function'
  ) {
    throw new Error('renderer isolation or early preload failed');
  }
  const text = 'Grüße 🪲\0tail';
  const result = await desktop.roundtrip({ text, nested: { value: 42 } });
  if (result.text !== text || result.answer !== 43) {
    throw new Error('IPC reply changed');
  }
  document.title = 'LINUX_RUNTIME_OK';
} catch (error) {
  document.title = `LINUX_RUNTIME_ERROR: ${error.message}`;
}
