# npm package measurements for Mite 0.1

Verified on macOS ARM64 with the native Release Mite executable and Node v24.15.0.
The [Linux x64 baseline](../development/linux-build.md) also passes the enabled
real-package/runtime suites in Release at ff51eab; later profiles need their own gate.
Dependencies come from `tooling/npm-fixtures/package-lock.json`; npm ci verifies
its locked integrity values, and package installation scripts are disabled.
The eleven focused application fixtures pass with identical stdout, stderr and exit
status in Mite and Node. They execute the published package source through the
normal CJS/ESM loader and node_modules layout. Full upstream package test suites
and native Windows execution remain unverified; Linux evidence is limited to the baseline above.

| Package | Version | Status | Measured behavior | License |
| --- | --- | --- | --- | --- |
| once | 1.4.0 | partial | Wrapper caching, called/value, callback property preservation, strict duplicate-call errors, CJS imports from ESM and dependency lookup | ISC |
| wrappy | 1.0.2 | partial | Transitive resolution, wrapper execution/property preservation via once, direct require identity and metadata | ISC |
| escape-string-regexp | 5.0.0 | partial | Native ESM default export, escaping punctuation/backslashes/hyphens, Unicode regex round-trip and invalid input TypeError/message | MIT |
| @msgpack/msgpack | 3.1.2 | partial | Published CJS and explicit ESM entries; long Unicode strings call native encodeInto/decode, binary arrays, integer/BigInt/timestamp round trips, golden bytes, async chunk/multi/array decoding and malformed/prototype-key/length-limit errors | ISC |
| @nodelib/fs.stat | 2.0.5 | partial | Published CJS source via require and ESM default import; sync/callback native file/directory stats, followed/unfollowed/marked symlinks, broken/missing entries, Settings reuse and adapter override | MIT |
| eventemitter3 | 5.0.1 | partial | CJS and conditional ESM entry identity, named/default ESM exports, symbol events, once/context/listener counts and timer-driven Promise completion | MIT |
| time-span | 5.1.0 | partial | Published ESM entry samples native process.hrtime.bigint; timer-spanning millisecond/rounded/second/nanosecond methods, units and five original-clock delegations pass locally against Node | MIT |
| convert-hrtime | 5.0.0 | partial | Unmodified ESM dependency converts actual native BigInt elapsed values; units and BigInt nanoseconds exercised through time-span | MIT |

The new time-span workflow is locally measured on macOS and still needs fresh
hosted platform acceptance. Its counting wrapper delegates every call to the original
native BigInt clock; neither package source nor sampled clock values are replaced.

The once README advertises a proto() helper, but this locked published version
exposes no such function under Node v24.15.0. The exports fixture confirms that
Mite matches the absent property and resulting TypeError. No dependency source
was patched to make the tests pass, and prototype support is not claimed.

The MessagePack fixtures wrap the encoder/decoder prototype methods only to count
calls and delegate directly to the originals; both native paths must be exercised.
They do not replace codecs or edit package source. The bare package entry is CJS;
the ESM fixture explicitly imports its published dist.esm entry. Async iterator
chunks are tested; this package fixture does not establish complete stream compatibility.
Mite now has separate partial [Node](../api/stream.md) and [Web Stream](../api/web-streams.md) contracts.

The filesystem package cases create isolated file/directory/symlink fixtures; they
do not mock the native filesystem. Symlink creation requires host privileges,
and native Windows execution remains unverified; Linux baseline scope is recorded above. No package source is edited.

License notices remain in the installed package directories. The runner verifies
that the installed versions match the lock and that their upstream license files
are present. The fixture dependencies are not embedded into Mite. Future binary
packaging must separately collect the native/runtime dependency notices.

Reproduce from the repository root:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
python3 tooling/tests/test_npm.py --mite build/dev/bin/mite
```

Actual development verification used `build/debug/bin/mite` with the same build
settings as the documented dev preset. This matrix measures a small package
corpus; it is not general npm or Node compatibility. Expand it with filesystem,
networking and Electron migration packages as those host APIs become available.
