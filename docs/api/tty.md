# node:tty

Status: partial. `isatty(fd)` uses actual native descriptor classification through
libuv. It accepts integer numeric descriptors from 0 through 2,147,483,647;
invalid types/ranges, closed descriptors, pipes and regular files return false,
without coercion. A real terminal returns true. On Windows, invalid CRT descriptors
are checked under a scoped invalid-parameter handler before querying libuv.

```js
import { isatty } from 'node:tty';
console.log(isatty(process.stderr.fd));
```

No terminal is opened or its mode changed by this query. `ReadStream` and
`WriteStream` construction explicitly reject; raw mode, terminal-owned streams,
window-size events and color-depth APIs are not implemented. Do not infer terminal
or color metadata from the presence of this module.

The focused Node/Mite fixture passes real pipe/file/closed/invalid descriptor cases
and an actual headless PTY on macOS. Native Windows/Linux acceptance remains open.
No desktop window is used by this test. The public baseline is
[Node 24 TTY](https://nodejs.org/download/release/v24.15.0/docs/api/tty.html).
