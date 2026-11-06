# Build a CLI tool

Prerequisite: a source-built Mite executable on the tested host. Follow
[getting started](../getting-started.md). Commands below run from the repository root;
use the executable path from your build directory.

## Run a script with arguments

```sh
build/dev/bin/mite examples/mite-cli/hello.mjs world
build/dev/bin/mite examples/mite-cli/modules.mjs world
build/dev/bin/mite examples/mite-cli/hash.mjs
```

Arguments after the entry file are script arguments. Use .mjs for explicit ESM,
.cjs for CommonJS, or package.json type for .js. See [modules](../concepts/modules-and-packages.md).

## Read and hash a file

Save this as hash-file.mjs:

```js
import {readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';

const filename = process.argv[2];
if (!filename) {
  console.error('Usage: mite hash-file.mjs <file>');
  process.exit(9);
}
try {
  const bytes = await readFile(filename);
  console.log(createHash('sha256').update(bytes).digest('hex'));
} catch (error) {
  console.error(error.code || error.message);
  process.exitCode = 1;
}
```

```sh
build/dev/bin/mite hash-file.mjs README.md
```

The output is one SHA-256 hexadecimal digest. A missing argument exits 9; a failed
read reports an error and exits nonzero. This uses supported Promise filesystem,
Buffer and incremental hash APIs. The repository's [hash example](../../examples/mite-cli/hash.mjs)
also demonstrates independent hash copies.

## Environment and output

Read environment values from process.env and use process.cwd/chdir when necessary.
Choose explicit paths for tools that write files; for exclusive output use the
supported wx flag. console.log/error convert values to strings and flush lines;
Node's object-inspection/format substitution, process stdio streams and interactive
readline/TTY APIs are not implemented.

## What to check next

Use the [CLI reference](../api/cli.md) for flags/exit behavior and
[Node migration guide](../migration/from-node.md) for package auditing. Do not pass
Node/V8 inspector, loader or TypeScript flags expecting them to work in Mite.
