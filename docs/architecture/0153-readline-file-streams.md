# ADR 0153: Real nonterminal readline over native FileHandle streams

Status: locally tested partial APIs and real Code-OSS startup advancement.

## Context

The actual ready/main graph next imports readline. Code-OSS osReleaseInfo.ts reads
Linux identifiers via for-await over createInterface({ input: handle.createReadStream(),
crlfDelay: Infinity }). Both a real line interface and the missing native file-stream
bridge are required. Server prompts also use callback question. Do not accept the
import while leaving these operations inert or fabricate Linux support on macOS.

## Decision

Add authored ESM parser, iterator and EventEmitter interface modules. Reuse the
existing StringDecoder, stream, native AbortSignal and event-loop facilities.
Frame UTF-8, LF/CR/CRLF and final lines incrementally. Match the observed truncated
UTF-8-tail behavior. Use elapsed CRLF delay, while keeping adjacent pairs together
across pressure. Limit individual chunks/lines and throttle iterator delivery through
real input.pause/resume, retaining only pending text plus a bounded line queue.
Preserve consumer exceptions rather than reclassifying them as parser errors.
Account for reentrant pause/resume, EOF, abort and pending read retirement.

Implement only nonterminal interface lifecycle/prompts/questions. Refuse TTY editing,
completion/history, cursor/key APIs and unsupported options. Document cleanup
semantics: Mite closes the interface on iterator return/error, including falsy error
values; the measured Node 24.15.0 return case retains its input listener. Interface
close leaves stream ownership with the caller. Do not claim complete overload,
terminal, Promise API, proxy/foreign-realm or event ordering parity.

Add a separate native FileHandle-to-Readable adapter using bounded existing read
operations. One read is active per stream; demand drives further chunks. Retain the
actual FileHandle, preserve range/position behavior and distinguish EOF auto-close
from explicit destroy. Even autoClose:false destruction closes the handle, as the
Node reference demonstrates. Wait for native reads before descriptor retirement.
Supply a private close callback that awaits the FileHandle's pending native close,
including after fd retirement, while preserving public repeated-close Promise identity. No new native filesystem backend,
LibWeb dependency or Ladybird change is introduced.

## Evidence and next work

Node/Mite/native-host tests cover byte boundaries, Unicode, CRLF timing, EOF,
lifecycle/question/abort, queue pressure, consumer exceptions and explicit limits.
Real file ranges, descriptor retention/destruction/manual-close races, forced GC and
30,000-line iteration pass. Pin the actual OS-release source and compile it with the
existing truthful platform overlay: its real macOS branch passes on both hosts;
the Linux file branch remains subject to native Linux execution in the CTest.

The rebuilt Firefly actual ready/main graph passes readline and next reports an
invalid/missing MessageChannelMain export. There is still no accepted editor window
or open/edit/save workflow. Prioritize the real Electron main/renderer MessagePort
contracts next, preserving binary data, endpoint/process identity and revocation.
Keep all original platform/extension/PTY/webview/remote requirements active. Fresh
Linux/Windows acceptance and full filesystem/readline parity remain unverified.
