# Native Windows bring-up

Windows support remains unverified. Native Windows development Actions builds Mite
on Windows Server 2025 with Visual Studio clang-cl x64, the pinned Rust toolchain
and pinned vcpkg source/dependencies. No WSL, MinGW compiler or alternate JS engine
is used as a runtime/build fallback. Node is only the differential test reference.

The first job failed before compilation because the DNS NUL-input fixture had the
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
exports only its required toolchain variables. pkg-config is a native prerequisite
as documented by upstream (the CI installs pkgconfiglite 0.28, with up to three
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
python3 tooling/test_checkout_paths.py
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

A successful native compile and complete measured runtime suite, native filesystem/
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
