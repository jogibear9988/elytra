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
as documented by upstream (the CI installs pkgconfiglite 0.28). Configure/build logs,
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
