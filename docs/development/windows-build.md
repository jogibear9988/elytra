# Native Windows bring-up

The documented Mite runtime subset now passes native Windows CI; full Windows
Firefly/desktop/package support remains unverified. Native Windows development Actions builds Mite
on Windows Server 2025 with Visual Studio clang-cl x64, the pinned Rust toolchain
and pinned vcpkg source/dependencies. No WSL, MinGW compiler or alternate JS engine
is used as a runtime/build fallback. Node is only the differential test reference.

Historical bring-up: the first job failed before compilation because the DNS NUL-input fixture had the
reserved basename nul.mjs. It is now null-byte.mjs; its contents and assertions are
unchanged. The next Windows run passed source/submodule checkout and native compiler
environment setup and reached CMake dependency configuration. This proves those
setup stages only, not a working Windows Mite binary or Firefly application.

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


## Experimental native LibWeb renderer gate

The independent Experimental native Windows renderer workflow now configures the
actual pinned Ladybird graph with the owned Firefly diagnostic targets, using
Visual Studio clang-cl x64, Rust 1.98.0 and CMake 4.2.0 on Windows Server 2025.
It compiles LibWebView, helpers and realm/wire probes, then requires real DOM,
absent Node globals, compositor PNG and actual isolation/wire assertions. Upstream's
native console/DLL deployment helper is applied to our diagnostic executables.

Compilation/execution is pending. This is a first headless component gate, not a
Windows desktop implementation or app package. The existing renderer runner keeps
Windows process/sandbox observations explicitly UNVERIFIED. Product target guards
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
