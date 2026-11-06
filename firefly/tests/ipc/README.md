# Native request/reply acceptance

All maintained main, preload and browser sources are ESM. The Python runner stages
fresh runtime-specific copies, bundles preloads to the supported sandbox format
with the pinned compiler, and loads browser modules directly from local files.
External file-module loading has its own renderer/security acceptance gate; IPC
fixtures no longer inline source as a workaround. HTTP-delivered modules also have
their Designer acceptance gate.

main.mjs compares the common subset against actual Electron. security-main.mjs,
budget-main.mjs, crash-main.mjs and timeout-main.mjs assert Firefly's deliberate
limits, navigation/revocation, real renderer loss, global reservations and actual
30-second deadline without test-only timeout settings. The crash harness verifies
the target PID is an owned WebContent child before killing that process.

Run through check-firefly or tooling/tests/test_public_ipc.py. Renderer/main code
cannot select a different native owner by supplying an ID in payload arguments.
