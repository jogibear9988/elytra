# 0049: Copy native network-interface snapshots into Mite

Status: node:os networkInterfaces validated on macOS ARM64. Native Windows/Linux,
interface reconfiguration and uncommon OS failures remain unverified.

## Decision

Use the existing libuv uv_interface_addresses API through our C++ OS adapter. Copy
all names, addresses, masks, MAC bytes, internal flags and IPv6 scope IDs into
rooted VM-owned objects before uv_free_interface_addresses. ScopeGuard releases
the native allocation even if JS result construction throws. No socket, worker,
new dependency, Node/V8, LibWeb coupling or vendored modification is required.

Return an ordinary object of per-interface arrays, containing fresh ordinary
records with address/netmask/family/mac/internal/cidr and IPv6-only scopeid. Preserve
the native enumeration order and Node's field order. Use libuv IP formatting,
lowercase six-byte MAC strings and UTF-8 interface names without BOM removal.
Group by decoded names using a native map; define own data properties rather than
consulting inherited properties for interface names. This also avoids treating
prototype-looking native names as inherited JS state; no OS interfaces were renamed
to exercise that unusual-name case.

Compute CIDR length directly from raw network-order mask bytes. A shared pure C++
helper counts contiguous leading ones and rejects a later one after a zero; invalid
masks produce cidr=null. IPv4/IPv6 byte order does not depend on host endian. Use
UV_ENOSYS -> empty object, matching the reference's unavailable-data policy. Other
native failures and unsupported address families raise the existing SystemError
path, instead of creating records with invented address metadata. These failure
branches were not forced on this host; unknown-family behavior is not Node parity.

## Evidence

The native mask probe tests 320 cases: every contiguous IPv4/IPv6 prefix, including
zero/full masks, and noncontiguous masks at every available bit boundary. It uses
only the standard C++ library and the production helper, without a JS VM.

The new mite-os-network differential suite compares actual Node 24.15.0 and Mite
snapshots. Twelve interfaces / 22 records match on the tested host, including IPv4/
IPv6 loopback and scoped IPv6 addresses. Python ipaddress parsing and an independent
integer mask formula validate address families, masks and CIDR. Prototype/property
shape/order, MAC syntax, internal/scope fields, ignored arguments/receiver, fresh
object/array/record mutation and CJS/ESM named identity pass. Samples are separate
native observations, not an atomic cross-process network configuration transaction.

All 31 standalone CTest suites pass in build/os-network-ctest.log. Firefly's main
ESM/HTTP fixture imports and executes networkInterfaces before renderer load; the
existing full Firefly/Electron gate passes in build/os-network-firefly-test.log.
Earlier OS/data, networking and isolation controls remain passing in their relevant
gates. No new renderer bridge or renderer OS capability is exposed.

## Limits

OS priorities/constants remain explicitly unsupported. Real non-ASCII/unusual names,
noncontiguous configured masks, unknown families, native syscall failures/ENOSYS,
network hotplug/reconfiguration, Windows/Linux and privileged execution were not
exercised. CIDR arithmetic is tested independently of those OS configurations.
Native structures are owned only during the query; returned data is a snapshot,
not a live interface monitor. No full Node metadata/error/prototype-poisoning parity,
new package/platform/packaging/audit or visible-desktop validation is claimed.
Existing deployment/Rust archive warnings remain. Ladybird's pin/source stay unchanged.

Reference behavior: official Node v24.15.0 [OS wrapper](https://github.com/nodejs/node/blob/v24.15.0/lib/os.js),
[native OS binding](https://github.com/nodejs/node/blob/v24.15.0/src/node_os.cc) and
[CIDR helper](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/util.js).
These implementation files are not embedded.
