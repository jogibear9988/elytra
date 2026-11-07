# 0107: Executable-owned Electron import policy

Use explicit --electron-compat opt-in instead of rewriting all application imports
or silently changing ordinary package resolution. Register the same actual API
object under both host names, before entry execution. Advertise only implemented
named exports; leave unsupported Electron APIs absent. Do not fake engine version
metadata. Mite stays independent of the desktop host.

Parse the executable option before the entry, then pass a borrowed, null-terminated
argv vector to Mite with the host option removed. Later application arguments cannot
change host policy. Package launchers compile the same explicit option from the
recorded packager choice. Native tests independently check argv placement, preserved
PID/arguments and relocation.

Propagate this bool through the trusted main/renderer preload configuration, not
through a page global or user-defined webPreferences. Preserve the existing exact
two-key metadata schema by default; the only accepted extra key is a typed
electronCompat boolean. A preload's require closure captures that immutable value,
keeps its weak isolated-world ownership/revocation checks and returns only the same
frozen contextBridge/ipcRenderer facade. There is no main API or filesystem grant.
Unknown metadata and unsupported require targets remain rejected.

The native macOS batch validates default/opt-in host and real owned renderer paths,
including a competing npm package and page/no-FS controls. Default app args contain
the same flag after the entry to prove they cannot opt in. Linux hosted acceptance
is required separately; Windows desktop is still unsupported. Pure parser and
package-launcher mechanics are not substitutes for that native renderer gate.

The actual full-source Code-OSS entry probe is now runnable in the real Firefly
host with explicit diagnostic shutdown. It remains nonzero and currently reaches
a missing minimist package after cache/original-fs/electron resolution. A complete
dependency graph, required exports/options and workbench startup remain unfinished.
See the [CLI contract](../api/firefly-cli.md). Ladybird source and IPC method IDs
are unchanged.
