/** Callback validation is synchronous and uses Node's argument error code. */
for (const value of [undefined, null, 1, {}, 'callback']) {
  try {
    setImmediate(value);
  } catch (error) {
    console.log(error.name, error.code);
  }
}
console.log(typeof __miteScheduleImmediate, typeof __miteControlImmediate);
