/** Ref state comes from pending native work, including after cancellation/firing. */
const discarded = setImmediate(() => console.log('UNEXPECTED'));
console.log(discarded.hasRef(), discarded.unref() === discarded, discarded.hasRef());
console.log(discarded.ref() === discarded, discarded.hasRef());
clearImmediate(discarded);
console.log(discarded.hasRef(), discarded.ref() === discarded, discarded.hasRef());
const disposed = setImmediate(() => console.log('UNEXPECTED'));
console.log(disposed[Symbol.dispose](), disposed.hasRef());
for (const value of [undefined, null, 42, 'x', {}, Symbol()]) {
  console.log(clearImmediate(value));
}
const handle = setImmediate(
  function (text, object) {
    console.log(this === handle, text, object.value, handle.hasRef());
    console.log(handle.ref() === handle, handle.hasRef());
  },
  'argument',
  { value: 42 }
);
