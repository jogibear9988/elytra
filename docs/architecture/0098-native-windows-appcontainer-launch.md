# ADR 0098: Native Windows AppContainer launch boundary

Status: implemented standalone component; native Windows acceptance pending.
Not selected by the product renderer. Windows desktop support remains unfinished.

## Context and language

The pinned Windows renderer sandbox entry point does nothing. Native window and
process-observer prototypes cannot establish containment. Launch restrictions must
exist before child code runs; applying them after a renderer is already running is
insufficient. Keep Ladybird unchanged and implement an owned adapter.

Use C++20 directly with Win32 for this resource/lifetime boundary. It needs no
LibJS, LibWeb, Qt, Rust FFI or vcpkg dependency. A small statically linked CRT probe
can therefore test actual kernel policy without waiting for the browser SDK build.
This probe is a security component fixture, not a replacement renderer.

## Launch contract

Create a fresh random AppContainer profile, never reuse an existing profile or
add capabilities. The caller supplies absolute executable/working-directory paths,
arguments and an explicit unique allowlist of already-owned inheritable handles.
All three standard streams must be in that list. Reject invalid handles, duplicate
entries, embedded NUL and relative paths. Do not mutate caller inheritance flags.
The caller must also prevent other concurrent unrestricted process launches from
inheriting these handles; this component's explicit list protects its own launch.

Replace the ambient environment with Windows system paths only. Quote arguments
according to Windows CRT rules and pass the exact executable to CreateProcessW.
Set SECURITY_CAPABILITIES, HANDLE_LIST and JOB_LIST startup attributes. The job has
KILL_ON_JOB_CLOSE and a one-process active limit, verified by a kernel query.
Assignment during creation avoids a broker-death gap between process creation and
job assignment. Return the child suspended, verify membership of the exact job and
require one explicit resume. Never retry as an ordinary unsandboxed process.

RAII owns process/thread/job handles and profile references. Closing the job kills
remaining children; failed post-creation validation also terminates the suspended
child. Profile removal is refused while any Child owns it, including suspended or
exited-but-still-owned children. Synchronize removal/retention. Explicit profile
cleanup failure remains visible. Unexpected broker death leaves profile cleanup to
the owning supervisor; profile deletion alone is not process termination.

This is an ordinary AppContainer, not LPAC. Default system/profile-access rules
still apply. No claim of zero filesystem access, complete exploit mitigation,
registry isolation testing or renderer containment follows from zero capabilities.

## Native acceptance

The independent Windows workflow builds the real broker/worker with clang-cl,
then runs Python as an external Win32 observer. Before resume, require the actual
child image and native parent, an AppContainer primary token, zero capability
SIDs, low-or-lower integrity and job membership. Retain raw evidence.

Only a fresh owned fixture tree receives explicit DACL changes: the package SID
gets read/execute resources and a read-only file; private data has no package grant.
No user/system ACL, firewall rule or loopback exemption is changed. Require actual
allowed reads and denied private reads/writes, exclusion of a deliberately
inheritable private handle, omission of a parent secret, denied child creation and
WSAEACCES connecting to an independently live loopback listener. API/query failures
are not successful denials. Invalid launch contracts and removed-profile reuse
must fail explicitly.

Kill the broker while its child is still suspended and again after the child
confirms actual execution. Hold the exact native child object across broker death
and require it to terminate. PID reuse cannot satisfy this test. Remove only the
uniquely named owned profile, preserve logs and clean up the fresh fixture tree.

## Required integration

Native component success is required, but does not enable Windows Firefly.
Integrate this launch boundary into an owned LibWeb process adapter, define the
actual renderer/helper IPC handles and resource/DLL grants, and test real LibWeb
under that policy. Network requests must remain brokered; do not enable broad
capabilities to make startup pass. Define compositor/helper topology and supervised
profile cleanup. Require actual desktop/preload/IPC/security, Designer and native
package gates before advertising a supported Windows product.

References: Microsoft's [AppContainer launch guide](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer),
[isolation scope](https://learn.microsoft.com/en-us/windows/win32/secauthz/appcontainer-isolation)
and [startup attributes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).
