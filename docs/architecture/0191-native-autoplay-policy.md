# ADR 0191: Native autoplay policy for Code-OSS

Status: native media/WebAudio enforcement and public option wiring implemented.
Thirteen media groups and ten WebAudio groups pass locally; the final inactive
fixture/GC checks and actual application transport acceptance remain unfinished.
Code-OSS does not run yet.

## Required behavior

The pinned Code-OSS constructor requests user-gesture-required. Electron 45.2.3
uses Chromium 156.0.8078.80; its
[autoplay policy](https://chromium.googlesource.com/chromium/src/+/156.0.8078.80/third_party/blink/renderer/core/html/media/autoplay_policy.cc)
and [media call sites](https://chromium.googlesource.com/chromium/src/+/156.0.8078.80/third_party/blink/renderer/core/html/media/html_media_element.cc)
define an element-specific lock. A transient activation can unlock play, explicit
load, or an audible volume transition. Pause and source reload retain the lock.
Inaudible video has an exception; inaudible audio does not. Native media controls
authorize their element, and a local opener can permit one play without permanently
unlocking the child element. Adoption follows the source/destination policy.

These rules differ from the SDK's document-sticky activation, process-global
allowlist and file-origin exception. Keep those original rules for unmanaged
documents; explicitly configured native contexts must use their own policy.

## Native media decision

Store bounded, atomic policy snapshots by genuine canonical top-level navigable
ID. Do not use a document URL or origin as ownership. A local top-level BiDi
identifier may be a window-handle UUID, while a remote top-level representation
uses namespace/local ID. Use the latter canonical native ID consistently for
local documents and documents below remote roots.

Element state contains no GC references. Compose the actual constructor-side
initialization, destructor, adoption, play, explicit load, audible change,
native-controls and admission methods. The real destructor removes the native
address; initialization also prevents address reuse from inheriting an unlock.
Keep SDK class headers/layouts and every unrelated source byte unchanged.

The local native test executes actual LibWeb/LibJS play promises and pause states,
native activation/consumption, Page media-menu dispatch, document adoption,
remote-root hosting, auxiliary opener behavior and collection. It uses no playable
source, Qt window, OS input or audio output. This does not certify decoding,
physical input, production policy transport or another operating system.

## WebAudio and public admission

The [pinned Chromium WebAudio implementation](https://chromium.googlesource.com/chromium/src/+/156.0.8078.80/third_party/blink/renderer/modules/webaudio/audio_context.cc)
also gates cross-origin-to-top contexts under user-gesture-required. The SDK Rust
AudioContext currently always starts. Its allowed_to_start is immutable, renderer
creation occurs only in the constructor, and blocked resume paths were unreachable.
Changing that boolean alone cannot implement the required lifecycle.

The two explicit values now have native implementation and experimental public
wiring. The implementation and remaining acceptance cover these requirements:

- Gate before output acquisition, with genuine lazy renderer creation shared by
  construction and the first authorized resume.
- Query real transient activation at construction, resume and source start; latch
  successful context authorization. Unrelated Window activation must not start
  pending contexts. Cover both scheduled-source and buffer-source start paths.
  Respect explicit user suspension, closed/inactive contexts and actual
  context/document ownership.
- Settle and release queued resume promises on success, failure, closure and
  revocation; never report running without a real renderer/graph.
- Carry trusted policy before the first author script for BrowserWindow and
  WebContentsView, including child processes and navigation. Enumerate owned
  default contexts too, and fail closed for missing/removed owned contexts while
  preserving ordinary unmanaged SDK behavior.
- Test actual contexts with Media::AudioOutputFactory::create_null and count
  acquisition calls. A restricted constructor/resume must acquire none; authorized
  resume/source start during transient activation must start the real null graph
  and settle normal media tasks.

Route the pinned Rust crate root and web_audio module to an owned AudioContext
source composition, preserving Cargo manifests, dependency features and notices.
Ordinary cargo test excludes WebAudio in this SDK and cannot prove this behavior.
BFCache renderer restoration is separately incomplete upstream.

document-user-activation-required additionally needs its own document/ancestor,
delegation and browser-exception semantics; keep it explicitly unsupported until
implemented. The two experimental public values use trusted native snapshots for
BrowserWindow and WebContentsView; no value is implemented through page-script
patches. Actual application transport and workbench/open-edit-save acceptance
remain separate gates.

## Current validation boundary

The real Release Rust crate and native Firefly host compile. Media admission,
metadata stripping, explicit owned defaults and fail-closed removed contexts pass
thirteen actual component groups. Ten genuine null-audio graph groups pass,
including transient construction/resume, both source-start paths, explicit user
suspension, authorization revisions, queued publication revocation and close.
The remaining native fixture currently fails its inactive-document pending-promise
assertion; its following GC check has not run. Keep that failure visible rather
than reporting the whole audio target as passing. No OS audio device is opened.

Queued tasks revalidate native policy without sampling a new activation. Removed
or changed settings cannot reuse an old context authorization. Public settings are
immutable; changing an already-playing graph's policy is outside this slice.
