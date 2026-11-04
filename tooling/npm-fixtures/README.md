# Real npm compatibility fixtures

Install the exact locked dependency tree with the existing npm package manager:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
python3 tooling/test_npm.py --mite build/dev/bin/mite
```

The test runner invokes each case with Node and Mite against the same node_modules
layout, comparing stdout, stderr and exit status. It fails with an installation
instruction if a package is missing or differs from the lock. Installation scripts
are disabled. Package versions, registry tarball URLs and SHA-512 integrity values
are committed in package-lock.json. Installed sources and license files stay in
ignored node_modules directories. These dependencies are development test data;
Mite's executable uses LibJS and does not invoke Node/npm.

| Package | Version | License | Use |
| --- | --- | --- | --- |
| once | 1.4.0 | ISC | Direct fixture dependency |
| wrappy | 1.0.2 | ISC | Transitive once dependency and direct test coverage |
| escape-string-regexp | 5.0.0 | MIT | ESM package fixture |
| eventemitter3 | 5.0.1 | MIT | Conditional import/require exports and event behavior |

The runner requires each installed upstream license file to remain present.
See `docs/compatibility/npm-packages-0.1.md` for the measured scope and limitations.
