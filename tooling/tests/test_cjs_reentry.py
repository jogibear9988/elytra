#!/usr/bin/env python3
"""Never publish competing native module records when a resolve hook reenters CJS analysis."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for the reference failure observation")
    with tempfile.TemporaryDirectory(prefix="mite-cjs-reentry-") as directory:
        root = Path(directory)
        (root / "entry.cjs").write_text("exports.a=1;module.exports=require('./leaf.cjs');\n")
        (root / "leaf.cjs").write_text("exports.b=2;\n")
        driver = root / "main.mjs"
        driver.write_text("""
import {registerHooks} from 'node:module';
let nested, started=false;
const hook=registerHooks({resolve(specifier,context,next){
  if(specifier==='./leaf.cjs'&&!started){started=true;nested=import('./entry.cjs');}
  return next(specifier,context);
}});
const first=await import('./entry.cjs');
const second=await nested;
const third=await import('./entry.cjs');
if(!started||first!==second||second!==third||first.default.b!==2||
   first.default!==first['module.exports'])throw Error('Canonical CJS namespace lost');
hook.deregister();
console.log('PASS canonical native module identity across reentrant analysis');
""")
        reference = subprocess.run(
            [args.node, str(driver)], capture_output=True, text=True, timeout=15
        )
        assert reference.returncode == 1 and "ReferenceError" in reference.stderr, reference
        assert "Export 'b' is not defined" in reference.stderr, reference
        result = subprocess.run(
            [str(args.mite.resolve()), str(driver)], capture_output=True, text=True, timeout=15
        )
        assert result.returncode == 0 and not result.stderr, result
        assert (
            result.stdout == "PASS canonical native module identity across reentrant analysis\n"
        ), result
    print("PASS reentrant CJS analysis preserves one native record without aborting")
    print("Explicit difference: Node24 rejects this reentry; Mite keeps the first namespace record")


if __name__ == "__main__":
    main()
