# Native Windows bring-up

Earlier scoped Mite baselines passed native Windows CI; the current review runtime
is not accepted on Windows. Full Firefly/desktop/package support remains unverified.
Candidate `49386ba`, [run 44048584702](https://github.com/jogibear9988/elytra/actions/runs/44048584702),
compiles with clang-cl and passes **159 of 160** runtime suites. Its only failure
is native stdio backpressure: the portable Writable provider calculated admission
before a synchronous write released its bytes. The narrow Node24 admission fix
now passes actual macOS Mite/embedded-host and both Node references; a fresh full
Windows run remains required. No assertion was waived. The older `05149fe` result
was 151/160; it is superseded as a failure inventory, not a green baseline.

Windows uses fixed Node24.22.3 because Node24.15 has a separately observed native
TCP-connect crash there. Linux/macOS use Node24.15.0. Do not downgrade that Windows
pin or waive assertions to hide newer reference differences. Follow-up commits
fix modern runtime contracts and independently classified test assumptions. Both
pinned references now pass the affected local macOS controls; Windows-only pipe
behavior still requires actual hosted acceptance. Fresh Windows CI is mandatory
before claiming this review runtime accepted, and native Firefly desktop/package
support remains unverified.

Native Windows development Actions builds Mite
on Windows Server 2025 with Visual Studio clang-cl x64, the pinned Rust toolchain
and pinned vcpkg source/dependencies. No WSL, MinGW compiler or alternate JS engine
is used as a runtime/build fallback. Node is only the differential test reference.

At 678c85e, [run 42376107074](https://github.com/jogibear9988/elytra/actions/runs/42376107074)
passes all 68 enabled suites (124.04 seconds), including Unicode host startup,
module utilities, raw host filesystem and performance/Code-OSS module tests.
Later slices need separate acceptance. At the earlier 8f617c0,
[run 42373435149](https://github.com/jogibear9988/elytra/actions/runs/42373435149)
compiled the native runtime but failed two tests: the new module utility fixture
incorrectly assumed its C: temp path and D: checkout shared a current drive, and
the Node reference exited with 0xC0000409 in HTTP default-errors (Mite completed).
The drive-aware fixture is corrected; the separate Node crash remains to diagnose.
Do not count that run as full runtime acceptance.

Windows native callers must pass UTF-8 arguments to Mite::run. CLI and embedded
test host now share WindowsArguments, which owns converted strings/pointers from
wmain's UTF-16 arguments and rejects invalid UTF-16. The host must not rely on the
CRT's narrow main argument encoding; libuv's Windows uv_setup_args returns those
arguments unchanged. CLI-only binary-output and handle-inheritance policies remain
at its entry point. The shared entry adapter and Unicode host cases pass in the
678c85e native gate above.

Historical bring-up: the first job failed before compilation because the DNS NUL-input fixture had the
reserved basename nul.mjs. It is now null-byte.mjs; its contents and assertions are
unchanged. The next Windows run passed source/submodule checkout and native compiler
environment setup and reached CMake dependency configuration. This proves those
setup stages only, not a working Windows Mite binary or Firefly application.

## Current repair snapshot — 2026-11-08

The d5de3d9 native runtime run compiled and passed 103 of 109 enabled suites, with
six failures; it is not a green baseline. Its renderer build failed in our crash
Reporter adapter because Core::System::stat is unavailable on Windows. The current
repair uses portable native UTF-8 path admission and requests overlapped child
handles for raw duplex fd3 IPC, retaining raw framing rather than enabling libuv
handle-transfer framing.

The test repairs preserve functional assertions: canonical working directories
avoid Node's short/long path alias mismatch, cleanup retries wait only for the
already bounded shell descendant, watcher diagnostics retain actual native event
names, and final reclamation still requires both weak references to disappear.
Collection for final ownership checks now runs in the embedding driver between
VM turns; synchronous collection remains inside active callbacks/pending I/O.
These paths pass local macOS checks, but Windows reruns must confirm the fixes.
They do not establish native Windows Firefly windows, crash capture or packaging.

## Run and inspect

Push implementation changes directly to master as requested, or use Actions →
Native Windows development → Run workflow. The workflow runs source-path checks
first on Ubuntu, then the actual build/test job on Windows. The Ubuntu job is only
a checkout guard, not Linux runtime validation. After native prerequisites:

```powershell
python tooling/bootstrap.py
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_TOOLCHAIN_FILE="$PWD/build/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build/windows --target mite --parallel 3
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
ctest --test-dir build/windows --output-on-failure
```

Use a native Visual Studio x64 developer environment; the shared workflow action
exports its required toolchain variables. It also enables
[Python UTF-8 mode](https://docs.python.org/3.12/using/cmdline.html#envvar-PYTHONUTF8)
for fixture files and captured runtime text, independent of the runner's ANSI
locale. This does not change the native CLI's argument conversion or filesystem
behavior. pkg-config is a native prerequisite as documented by upstream
(the CI installs pkgconfiglite 0.28, with up to three
attempts and 15/30-second backoff for feed outages). Installation still fails the
job if all attempts fail; the installed executable must also run. Configure/build logs,
vcpkg buildtree logs and test output are retained as diagnostic artifacts.
Runs on the same ref are serialized so an in-flight native dependency build is not
interrupted by another bring-up commit. Completed
vcpkg binary packages are cached even if later compilation fails, avoiding repeated
dependency builds during bring-up. There are no continue-on-error success paths.

## Checkout guard

```sh
python3 tooling/check_checkout_paths.py
python3 tooling/tests/test_checkout_paths.py
```

The guard reads Git's tracked index rather than walking the worktree. It rejects
reserved Win32 component/device names (including extensions), reserved/control
characters and trailing periods/spaces. It ignores untracked user/build files.
Tests exercise the actual former nul.mjs regression, valid neighboring names,
directory components, superscript device numbers and an isolated tracked-file index.
The rules follow [Microsoft's filename conventions](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file).
This is a focused checkout guard, not a full NTFS/case/long-path validator or a test
of runtime path APIs. The pinned Ladybird tree was separately audited with no such
invalid component found; no upstream file is renamed or excluded.

## Still required

The complete measured runtime suite, remaining native filesystem/
network/process behavior, Firefly window/render-process/sandbox adapters, native
Designer acceptance, dependency deployment and Windows packaging remain required.
The release workflow must fail until all its native requirements pass. Never replace
the engine, relax renderer isolation or publish a macOS-only success as a complete
macOS/Windows release. Follow the upstream update guide and keep source unmodified.

## Filesystem compile follow-up

Run 42121184088 (source 1c5f535) passed the LF integrity gate and reached native
compilation, then rejected POSIX-only S_IFSOCK/S_IFBLK and O_SYNC identifiers in our
filesystem adapter. Optional device-type exports/cases are now guarded, and the
sync-open value comes from libuv's portable UV_FS_O_SYNC (FILE_FLAG_WRITE_THROUGH
mapping on Windows). No invented device type or ignored synchronous-open flag is
used. Local Release compilation and filesystem differentials pass; the next actual
Windows job must establish native compilation and behavior.

Node's [constant exports](https://github.com/nodejs/node/blob/v24.15.0/src/node_constants.cc)
are likewise conditional for unavailable native file types. This is a compile
portability fix, not a Windows runtime/renderer/package support announcement.

## CRT header shadowing

Run 42125657167 reached object 297/300, then the embedded-host test's standard
thread header could not find _beginthreadex. Our generic Process.h shadowed the
Windows CRT's process.h on the case-insensitive include search path. Rename the
private header to ProcessHost.h and update its three includes. Do not patch the
CRT, reorder system headers as a workaround or drop the embedded-host target.
Local Release build plus both host-module/embedding suites pass; the next native
Windows run must confirm the real CRT lookup and complete remaining runtime gates.

## First linked runtime and native startup fault

Run 42130072857 (390b86d, job 127604194189) successfully compiles and links Mite
and its test hosts after the header and Rust-archive fixes. Its runtime suites
then fail: VM initialization exits with Windows access violation 0xc0000005,
including the simple -e path. Help/version paths work without constructing a VM.
This establishes native linking, not a working JavaScript runtime. Do not report
the many affected suites as independent supported-API regressions before resolving
their common startup failure.

The Windows-only mite-startup-probe adds bounded DbgHelp exception-context stack
reporting to the test host, not the shipping CLI. It first exercises a deliberately
raised fault as a negative control, then runs a real Mite evaluation and preserves
failure status. The development job enables embedded C++/Rust symbols and retains
the report/PDB. Native compilation and symbolized output of this new probe still
need their next actual job. No engine patch, ignored exception or alternate runtime
is used to turn a crash green.

Separately, the uninitialized-submodule guard fixture attempted to delete read-only
Git object files. Moving the fixture's .git directory out of repository discovery
preserves the same rejection assertion and lets TemporaryDirectory own cleanup.
Its seven tests pass locally; Windows acceptance remains pending.

Run 42134241990 compiles the corrected reporter and passes seven of 38 suites,
including the real VM/realm null-deallocation SDK probe and revised Git fixture.
The remaining Mite startup fault is therefore narrower than basic engine creation.
The deliberate reporter fault yields symbolized frames, while actual Mite startup
has instruction pointer/address zero. The reporter now reads one bounded return
candidate safely from the captured stack, requiring a loaded module, before trying
to unwind that candidate. A tail jump can differ, so this remains a candidate until
its call site is reviewed. Only the local unwind-context copy changes; the program
still terminates with the actual exception status. This new path awaits native CI.

The startup probe now runs immediately after compilation. A failed real evaluation
fails the job before spending time on npm installation and broad suites; those
suites must all run/pass once the startup gate succeeds. Diagnostic/integrity
artifacts remain unconditional. No skipped suite is treated as successful acceptance.

## libuv process-query startup order

Run 42136659095 at 9752348 resolves the null-PC candidate to uv_os_getppid,
called from create_process when defining process.ppid. The native source pin is
libuv 1.52.1: its Windows getter calls pNtQueryInformationProcess without running
the one-time initialization itself. uv_loop_init initializes those WinAPI function
pointers. Mite previously constructed its owned EventLoop after create_process.

Construct the same owned loop before process queries, keeping realm roots alive
through teardown. Do not call libuv private initialization functions or create a
throwaway default loop. This changes our ordering, not libuv/Ladybird source or
process.ppid semantics. A fresh-process behavioral control compares ppid against
the actual Python launcher PID under both Node and Mite. Native Windows acceptance
still requires the next startup/runtime job; local checks alone cannot certify it.

Runs 42138308876 (d9b1ec0) and 42138486271 (db0b611) could not test that fix:
the Chocolatey prerequisite feed returned HTTP 504 and 503 respectively, before
configuration/compilation. They are infrastructure failures, not new runtime traces.
The shared setup action now retries the same pinned install at most three times.
Actual startup and all runtime suites still require a successful native job.

## Unicode CLI argument boundary

The post-libuv Windows job 42139627506 passes actual startup and 25/38 suites,
but the Unicode argument fixture fails. The CLI used narrow main/argv, whose
Windows CRT conversion can lose characters outside the ANSI codepage before
Mite sees them. Use [wmain](https://learn.microsoft.com/en-us/cpp/c-language/using-wmain)
and convert each argument with
[WideCharToMultiByte](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte),
CP_UTF8 and WC_ERR_INVALID_CHARS. Own all bytes and the terminating argv sentinel
through run(). Embedded hosts still provide UTF-8 bytes explicitly.

The shared runtime suite now exercises non-ASCII eval source and arguments,
empty strings, quotes, spaces, trailing backslashes and combining characters.
A Windows-only malformed-surrogate check requires explicit rejection before JS.
Local macOS checks do not compile the Windows entry path; the next actual native
job must validate it. Unicode script/module paths, console output and remaining
filesystem/network differences remain separate work; no upstream patch is used.

## Native module filenames and Unicode entry paths

Use explicit UTF-8/native filesystem conversion when resolving and opening the
CLI entry, retaining the native filesystem path for file operations. Module
metadata now uses preferred native separators; file-URL construction separately
uses generic separators. Mixing those representations made import.meta filenames
and CommonJS module.paths disagree with Node on Windows.

The package test now uses path.join(__dirname, 'node_modules') rather than a Unix
separator concatenation. The file-URL error fixture checks a missing Windows drive
on Windows and an invalid nonlocal host on POSIX, each against Node. A separate
Windows-only policy test requires UNC module URLs to fail with
ERR_MITE_UNSUPPORTED_API; no unsupported UNC import is silently attempted and the
Node reference is not asked to probe a remote SMB host.

The new mite-module-paths suite creates real Unicode entry/ESM/CommonJS/npm-package
files, including escaped #/% URL components, and compares body/metadata/identity
with Node. These checks pass locally; native Windows runtime/path acceptance is
still pending. Native filesystem API differences remain independent of this loader
fix and are not normalized away.

### Module realpath spelling

Run 42143633063 at 4994a92 exposes a genuine metadata mismatch: Windows filesystem
canonicalization expands RUNNER~1 while Node's module resolver retains that normal
component spelling. The owned loader now uses component-wise libuv lstat/readlink
on Windows, preserving ordinary names while following symbolic links and directory
junctions. POSIX keeps its existing canonicalization. See
[ADR 0064](../architecture/0064-windows-module-realpaths.md).

The expanded differential suite checks linked entry metadata and shared CJS/ESM
cache instances, native short-name aliases when available, and mixed-case Windows
entry spelling. Python samefile verifies filesystem identity; no output is
normalized to mask differences. Local POSIX regression checks pass; native Windows
acceptance of this candidate remains pending.

The later native 6ba8af1 run passes initial CJS path metadata but exposes a separate
ESM URL difference: Node encodes the short-name tilde as %7E; the loader left it
literal, while Mite's public pathToFileURL already encoded it. A punctuation audit
also found raw square brackets instead of %5B/%5D. The loader now uses matching
escapes. A real tilde/bracket-containing directory reproduces the old
failure on POSIX too. The native metadata suite checks filename-to-URL equality
and shared module identity through raw/escaped tilde and bracket imports; Firefly's main ESM
workflow also imports a real tilde-named dependency both ways. New Windows
acceptance, including the previously unreached junction/short-alias checks, remains
pending. File-path spelling itself is preserved, and general URL parsing is unchanged.

## CLI stdio inheritance and detached-child diagnostics

The 6ba8af1 detached/unreferenced-child fixture times out while capturing Mite's
parent output. A capture timeout alone does not establish that the parent is
alive: a descendant can retain an inherited output handle after the parent exits.
Pinned libuv's Windows CreateProcess path inherits inheritable handles; its
recommended early uv_disable_stdio_inheritance call was missing from Mite's CLI.
The Windows wmain now calls it before stream-mode/argument setup. Explicitly
requested stdio:'inherit' still uses libuv's deliberate handle duplication.

The fixture now observes process exit and pipe EOF separately within the same
three-second acceptance deadline. An independent Python parent/child control
deliberately retains inherited capture pipes, proving that the harness detects
the distinction. Cleanup always acknowledges the owned child; its separate drain
deadline does not extend acceptance. No sleep/timeout increase masks the failure.
Local controls pass; native Windows acceptance and definitive attribution of the
original timeout still require the new hosted result.

This is CLI process-start policy. Embedded hosts retain control of their process
inheritance settings and should establish their policy before starting Mite or
duplicating inherited handles. The
[libuv call](https://docs.libuv.org/en/v1.x/process.html#c.uv_disable_stdio_inheritance)
is best effort, not a complete arbitrary-handle allowlist or sandbox guarantee.

## CLI console byte output

Native ef61124 run 42147228418 exposes an output-byte difference in the child
process fixture: Mite's console.log writes CRLF through a pipe while Node writes
LF. Python's ordinary text-mode capture hid this in earlier console tests.
The Windows CLI now sets its stdout/stderr CRT descriptors to binary mode before
printing diagnostics or entering the runtime. It leaves embedded hosts' stream
configuration to the host. A raw-byte behavior test covers LF, explicit CRLF,
NUL, SUB and UTF-8 on both output streams without newline conversion.
Local regression checks pass; native Windows acceptance remains pending. This
does not establish terminal code-page/rendering parity or add console formatting.

## Wide-entry runtime result and remaining contract fixes

Run [42141915944](https://github.com/jogibear9988/elytra/actions/runs/42141915944)
at 1df78c8 passes native compilation/startup and 32/40 suites in 62.18s. The runtime
behavior suite verifies Unicode eval/quoted arguments and malformed UTF-16
rejection. The randomInt and HTTP-client suites also pass. This is useful native
runtime evidence within a failed platform job, not full Windows acceptance.

The eight failures include older module/package, filesystem and drain fixtures,
for which subsequent commits provide candidate corrections. Additional output
identifies pathToFileURL trailing-separator differences at drive roots and foreign
path styles, and Windows tmpdir ignoring a replaced process.env object. Match the
reference's native path separator check and Windows JS environment branch while
retaining POSIX native environment lookup. Those new Windows branches still need
their next actual job.

Two harness failures were platform assumptions: the known Node hash assertion
returns 134 on Windows instead of POSIX signal status -6, and both runtimes correctly
strip a Windows drive prefix when resolving POSIX paths. The tests now check the
observed platform-specific abort status with its exact assertion marker and compute
the live POSIX cwd without a drive prefix. Mite must still produce the safe result;
no arbitrary reference crash or runtime mismatch is accepted.


## First complete runtime gate

[Run 42155631612](https://github.com/jogibear9988/elytra/actions/runs/42155631612)
at 745c7af passes native clang-cl x64 Release compilation, startup diagnostics,
all **43** enabled CTests in **49.23 seconds**, and the post-build Ladybird integrity
gate. This accepts the tested runtime subset, including canonical module paths,
raw console bytes, spawn/execFile and detached-child capture. No test timeout or
output normalization was relaxed. It supersedes the pending acceptance notes above
for those scoped corrections; it does not prove all Windows edge cases.

The earlier Node-reference DNS custom-lookup native abort did not recur in this
run. Its root cause is not established; retain the comparison and diagnose any
recurrence rather than treating unknown native exits as acceptable. This success
is not native Firefly windows/sandbox, Windows app packaging, an installer or a
published Windows release. Those requirements remain open.


## AppContainer network evidence on Server 2025

The small sandbox component compiles natively at `273d905`, but its first runtime
gate fails: the socket remains pending until WSAPoll times out instead of returning
WSAEACCES. All other reported isolation checks pass; that run is still red. A
connection timeout alone does not establish network isolation.

The revised probe retains the actual socket result and asks the native Windows
[network-isolation diagnosis API](https://learn.microsoft.com/en-us/windows/win32/api/netfw/nf-netfw-networkisolationdiagnoseconnectfailureandgetinfo)
inside that same AppContainer, for the exact attempted server, before socket/Winsock
cleanup. Only ERROR_SUCCESS plus an exact missing private/internet capability can
corroborate the pending connection. NONE, unknown results, missing/failed APIs,
connection refusal and successful sockets fail. The parent still independently
requires no connection at its live listener and verifies zero token capabilities.
Native predicate controls and an independent Python verifier cover those refusals.
This changes diagnostic evidence, not the sandbox policy or loopback exemptions.
Fresh hosted Windows execution remains pending; the OS may report no diagnosis and
correctly leave the gate red. This component does not accept real renderer startup.

The subsequent native `dfd71f2` run compiles the diagnosis code but returns
ERROR_SUCCESS with NONE. Its timeout therefore still fails; absence of a diagnosis
is never evidence of isolation. A probe-only parent WFP observer now separately
requires an exact AppContainer capability-drop event. Attribution uses the fresh
unique package SID, OS application identity, TCP loopback socket tuple and event
time inside the retained child's actual lifetime. WFP's event header has no PID;
the parent independently retains and verifies the child's handle/birth identity.
Only capability drops qualify, not generic firewall drops.

The observer reads existing event-collection state, subscribes while the child is
suspended, stores at most eight matching scalar records and drains callbacks before
retiring the child/profile. It does not change collection settings, firewall rules
or loopback exemptions. The child's raw timeout/NONE remains false with an explicit
unresolved exit status; only the separate exact kernel record can corroborate it.
Disabled collection, unavailable rights or no matching event keep acceptance red.
At `6939a9d`, the actual native Windows component compiles, runs the predicate
controls and successfully opens/subscribes to WFP with collection enabled. Its
acceptance still fails because the exact capability-drop list is empty. This is
useful compiler/API evidence, not a network-isolation proof or a green component.
The next diagnostic step inspects only bounded matching event/filter metadata;
missing evidence must never be converted into a successful denial assertion.
Read-only collection/keyword-mask values and saturated rejection-stage counters
now distinguish absent callbacks from event-field mismatches. Diagnostic records
can classify an unspecified pre-connect local address; that never relaxes the
strict accepted-event predicate. These observations still require native results.


The native `fc7d36c` observation identifies collection enabled but keyword mask
zero, with no callbacks. Dedicated hosted CI now explicitly selects the probe's
`--audit-capability-drop` option: temporarily OR the capability-drop audit bit into
the exact captured mask, verify it, unsubscribe/drain callbacks, then restore and
verify the original mask before reporting cleanup. The default remains read-only.
This changes diagnostic logging only; it never changes firewall rules, AppContainer
capabilities, collection policy or loopback exemptions. Strict event acceptance is
unchanged. Observed state drift or failed read/write/restore fails the probe.
Windows supplies no atomic compare-and-swap here, and abrupt process death cannot
guarantee restoration. This option is confined to isolated disposable CI machines,
not a production sandbox mechanism or a general concurrent audit-ownership claim.
Fresh native execution remains required.

## Owned helper and desktop-provider integration (2026-11-17)

The current review adds native Windows locale, monitor/cursor, Qt clipboard,
window centering and Unicode runtime-path providers. They can compile separately
from the full application. Read-only provider and relocation probes join the
native Windows renderer workflow. Source controls pass locally; their actual
Windows compilation/execution remains pending. Theme/accessibility, hidden title
bars and broader desktop behavior are not accepted by those components.

A second, explicit configuration selects
`--experimental-windows-helper-launch`. The default remains off. It composes only
reviewed, hash-pinned upstream helper spawn/Core lifetime and broker seams into
owned build outputs; the Ladybird checkout stays unmodified. One shared DLL owns
scoped launch authority and Job/profile/resource leases across SDK libraries.
Only WebContent is admitted; other roles refuse. The parent verifies the exact
AppContainer SID, zero capabilities, low integrity and its freshly owned Job
before resuming the child. A child never inherits the Job handle or a parent
`PROCESS_DUP_HANDLE` capability.

The component uses an explicit environment, selected socket takeover and trusted
Windows system paths, rather than copying ambient variables. Source audit confirms that origin/file,
permission and webRequest snapshots already travel through exact parent-owned argv
and are consumed before VM initialization; preload environment and document policy
use authenticated session IPC. Native Windows acceptance of those paths remains
required. ICU initialization occurs earlier, in a static constructor: the sealed
executable/DLL-relative share/Lagom/icu data must be present, without forwarding an
ambient LADYBIRD_ICU_DATA override. Resource staging requires
an exact-revision, hashed physical inventory, rejects hardlinks/reparse points,
retains write/rename/directory handles and verifies the complete inventory before
scoped SID ACL grants. Normal cleanup revokes its owned grants. Abrupt parent
death terminates Job children but may leave profile/ACL residue; automatic residue
recovery is not implemented.

The private broker rejects pseudo/truncated handles, rolls back partial duplicates
and bounds remote-owned tokens/handles with disconnect refunds. Browser-origin
retention deliberately remains unchanged: upstream synchronous send paths can
abort on rejected admission, so a global quota would introduce a receiver-driven
Browser failure. Real Browser sender backpressure/shutdown and retention control
remain required before complete desktop acceptance.

```powershell
python tooling/tests/test_windows_desktop_policy.py
python tooling/tests/test_windows_helper_source.py
python tooling/tests/test_windows_broker_source.py
python tooling/build_renderer.py --build-dir build/windows-renderer --target firefly-renderer-probe --experimental-windows-helper-launch --configure-only --jobs 3
cmake --build build/windows-renderer --target check-firefly-windows-broker --parallel 3
```

The native broker probe stages its own real Core/AppContainer test executable as
`WebContent.exe`; it is **not actual LibWeb/DOM/preload startup**. Its passing source
controls do not establish native token, ACL, race, process or broker execution.
The hosted native target must pass independently. Actual FireflyWebContent startup,
all other helper roles including network/loopback policy, native security-transport acceptance,
Browser broker flow control, WinMain/std-descriptor behavior and PE/DLL packaging
remain release blockers. The producer target's
`FIREFLY_WINDOWS_HELPER_ROLES_COMPLETE` property stays false; an unrelated cache
boolean cannot open the full runtime gate. No Designer/Code-OSS Windows EXE is
advertised from this slice.

## Experimental native LibWeb renderer gate

The independent Experimental native Windows renderer workflow now configures the
actual pinned Ladybird graph with the owned Firefly diagnostic targets, using
Visual Studio clang-cl x64, Rust 1.98.0 and CMake 4.2.0 on Windows Server 2025.
It compiles LibWebView, helpers and realm/wire probes, then requires real DOM,
absent Node globals, compositor PNG and actual isolation/wire assertions. Upstream's
native console/DLL deployment helper is applied to our diagnostic executables.

Compilation/execution is pending. This is a first headless component gate, not a
Windows desktop implementation or app package. The renderer runner now requires independent Win32 executable/parent/creation-time
observations for WebContent, RequestServer and Compositor, and records actual
primary-token/job facts. The SDK's Windows RendererSandboxUnimplemented applies
no renderer sandbox; containment remains explicitly UNVERIFIED. The observer's
own native component gate uses a real owned Python child, not a renderer substitute. Product target guards
still reject unsupported Windows desktop/preload builds. No WSL, MinGW, Node/V8 or
alternate renderer substitutes for native acceptance.

```powershell
# Run in the native VS x64 clang-cl environment after bootstrap.
$env:CC = 'clang-cl'
$env:CXX = 'clang-cl'
python tooling/build_renderer.py --build-dir build/windows-renderer --target firefly-renderer-probe --configure-only --jobs 3
cmake --build build/windows-renderer --target firefly-renderer-probe firefly-isolation-probe firefly-wire-process-probe --parallel 3
python tooling/tests/test_renderer.py --probe build/windows-renderer/bin/firefly-renderer-probe.exe --artifacts-dir build/windows-renderer-artifacts
cmake --build build/windows-renderer --target check-firefly-isolation --parallel 3
```

The workflow preserves configuration/build/test logs, PNG and pinned dependency
buildtree diagnostics after failures. Native window/input, process/token/sandbox,
main Mite integration, desktop services, Designer and release packaging remain
required. See [ADR 0096](../architecture/0096-native-windows-renderer-bringup.md).


## Native Qt window prototype

The owned presentation build now selects the pinned Qt DirectX backend on Windows
and provides firefly-window-probe. Its native hosted gate remains pending. After
all headless/realm/wire gates succeed, renderer CI compiles it and runs the shared
native input/resize/navigation/close/paint assertions, requiring the actual windows
QPA platform. A second window observes actual HWND ownership and uses a verified
native WebContent handle to test renderer-loss failure. It does not enable the
unfinished production Firefly/Mite desktop runtime or its absent Windows sandbox.

```powershell
python tooling/build_renderer.py --build-dir build/windows-renderer --target firefly-window-probe --configure-only --jobs 3
cmake --build build/windows-renderer --target firefly-window-probe --parallel 3
python tooling/tests/test_window.py --probe build/windows-renderer/bin/firefly-window-probe.exe
```

These are native GUI commands intended for dedicated CI/testing, not the default
standalone Mite acceptance path. The prototype inherits upstream Qt native plugin
and DLL deployment in its development SDK; it is not a relocatable app package.
Physical/SendInput/DPI/driver coverage, main-process API integration, security and
Designer release work remain separate requirements. See
[ADR 0097](../architecture/0097-native-windows-qt-window-prototype.md).

## AppContainer launch component

The separate Native Windows AppContainer component workflow builds a small native
Win32 broker/worker without the browser SDK or a GUI. It requires a fresh
zero-capability AppContainer, explicit inherited handles, a clean environment and
atomic creation-time job assignment. Independent token/process observations precede
resume. Real denied file/network/process operations and broker-death cleanup are
required. Native acceptance is pending; this is not yet wired into LibWeb, and the
production Windows desktop guards remain in place.

```powershell
cmake -S firefly/native/sandbox/windows -B build/windows-sandbox -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build/windows-sandbox --parallel 3
python tooling/tests/test_windows_sandbox.py --probe build/windows-sandbox/firefly-windows-sandbox-probe.exe --artifacts-dir build/windows-sandbox-artifacts
```

The fixture changes ACLs only inside a fresh owned temporary tree and deletes
only its fresh profile. No loopback/firewall exemption or system ACL changes are
needed. Ordinary AppContainer default access still applies; this is not LPAC or a
complete renderer-containment proof. Actual IPC/resource integration and complete
Windows app/Designer/package acceptance remain required. See
[ADR 0098](../architecture/0098-native-windows-appcontainer-launch.md).


## Native package launcher

The Windows launcher expects three paths relative to the directory that
contains it.

```text
Launch.exe
bin/firefly.exe
app/main.mjs
```

It opens the explicit bundled image, preserves the caller's working
directory, forwards UTF-16 arguments and returns the actual child's exit
status. The build can opt in to the limited Electron import alias for
Code-OSS. The [native launcher job](https://github.com/jogibear9988/elytra/actions/runs/44178191931)
verifies both modes on Windows Server 2025 with a real instrumented child
in a relocated Unicode directory. It also checks missing bundled inputs
with an executable of the same name in the working directory. Packaging
the actual native engine, DLL closure and sample applications still
requires the full Windows runtime and isolation gates.
