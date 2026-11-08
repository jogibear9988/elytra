#!/usr/bin/env python3
"""Compare generated-parser CJS namespace classification with actual Node/Mite imports."""

import _support  # Standalone regression imports.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

# The source runs unchanged through each runtime's public CJS-to-ESM loader. There
# is no application evaluation during metadata discovery and no npm/scratch parser.
CASES = [
    (
        "unsafe getter alone",
        "Object.defineProperty(exports,'x',{enumerable:true,get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}})",
        [],
    ),
    (
        "prior assignment unsafe getter",
        "exports.x=1;Object.defineProperty(exports,'x',{enumerable:true,get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}})",
        ["x"],
    ),
    (
        "after assignment unsafe getter",
        "Object.defineProperty(exports,'x',{enumerable:true,get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}});if(false)exports.x=1",
        ["x"],
    ),
    (
        "prior value unsafe getter",
        "Object.defineProperty(exports,'x',{value:1,configurable:true});Object.defineProperty(exports,'x',{enumerable:true,get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}})",
        ["x"],
    ),
    (
        "nonrecognized getter then unsafe",
        "Object.defineProperty(exports,'x',{configurable:true,get(){return a}});Object.defineProperty(exports,'x',{enumerable:true,get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}})",
        [],
    ),
    ("setter alone", "Object.defineProperty(exports,'x',{set(v){}})", []),
    ("assignment then setter", "exports.x=1;Object.defineProperty(exports,'x',{set(v){}})", ["x"]),
    (
        "unsafe getter then replacement",
        "Object.defineProperty(exports,'x',{get(){globalThis.lexerEvents.push('unsafe');throw Error('x')}});module.exports={x:a}",
        ["x"],
    ),
    ("object numeric", "module.exports={x:1,y:2}", []),
    ("object string", "module.exports={x:'one',y:'two'}", []),
    ("object mixed", "module.exports={x:1,y:a,z:'three',q:b}", []),
    ("object identifiers", "module.exports={a,b,x:a,'y':b}", ["a", "b", "x", "y"]),
    ("object quoted numeric key", "module.exports={'1':a,2:b}", ["1"]),
    ("object computed literal key", "module.exports={['x']:a}", []),
    ("object method", "module.exports={x(){return 1}}", ["x"]),
    ("object spread", "module.exports={...{x:1},y:a}", []),
    ("object property read", "module.exports={x:Math.PI,y:a}", ["x"]),
    ("recognized getter throws", "Object.defineProperty(exports,'x',{get(){return dep.x}})", ["x"]),
    ("nested false assignment", "if(false){exports.x=1};module.exports={}", ["x"]),
    ("comment text", "/*exports.x=1;*/module.exports={}", []),
    ("quoted export name", "exports['a\"b']=1", ['a"b']),
    ("backslash export name", "exports['a\\\\b']=1", ["a\\b"]),
    ("newline export name", "exports['a\\nb']=1", ["a\nb"]),
    (
        "semicolon export name",
        "exports['x;globalThis.injected=true;//']=1",
        ["x;globalThis.injected=true;//"],
    ),
    ("surrogate export name", "exports['\\ud800']=1", []),
    ("paired surrogate export name", "exports['\\ud83d\\ude00']=1", ["😀"]),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node reference is required")
    with tempfile.TemporaryDirectory(prefix="mite-cjs-lexer-policy-") as directory:
        work = Path(directory)
        sources = []
        for index, (label, body, expected) in enumerate(CASES):
            source = work / f"case-{index}.cjs"
            source.write_text(
                "globalThis.lexerEvents=[];const a=1,b=2,dep={get x(){"
                "globalThis.lexerEvents.push('dep');throw Error('dep')}};" + body,
                encoding="utf8",
            )
            sources.append([label, source.as_uri(), expected])
        driver = work / "driver.mjs"
        driver.write_text(
            """
const cases=CASES;
const observations=[];
if(globalThis.injected!==undefined)throw Error('Unexpected prior injected state');
for(const [label,url,expected] of cases){
 const namespace=await import(url);
 const keys=Object.keys(namespace).filter(key=>key!=='default'&&key!=='module.exports').sort();
 if(JSON.stringify(keys)!==JSON.stringify(expected))throw Error(label+': unexpected namespace '+JSON.stringify(keys));
 if(namespace.default!==namespace['module.exports'])throw Error(label+': default marker identity');
 const events=globalThis.lexerEvents.slice();
 if(events.length>1)throw Error(label+': repeated getter snapshot');
 if((label==='unsafe getter alone'||label==='nonrecognized getter then unsafe')&&events.length)throw Error(label+': unsupported getter was evaluated');
 if(['prior assignment unsafe getter','after assignment unsafe getter','prior value unsafe getter','recognized getter throws'].includes(label)&&events.length!==1)throw Error(label+': recognized getter snapshot was not attempted');
 if(events.length&&namespace.x!==undefined)throw Error(label+': throwing getter snapshot must be undefined');
 if(globalThis.injected!==undefined)throw Error('Export-name source injection');
 observations.push({label,keys,events});
}
console.log(JSON.stringify(observations));
""".replace("CASES", json.dumps(sources)),
            encoding="utf8",
        )
        results = [
            subprocess.run([binary, str(driver)], capture_output=True, text=True, timeout=20)
            for binary in [args.node, str(args.mite.resolve())]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), results
        assert results[0].stdout == results[1].stdout, results
        observations = json.loads(results[0].stdout)
        assert len(observations) == len(CASES)
        for observation in observations:
            print("PASS actual Node/Mite CJS namespace:", observation["label"])
        print(
            "DONE CJS lexer policy: real imports and getter snapshots; no application-at-linking workaround"
        )


if __name__ == "__main__":
    main()
