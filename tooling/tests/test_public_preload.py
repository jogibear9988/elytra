#!/usr/bin/env python3
"""Run application-selected isolated preloads in actual Firefly and Electron windows."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import tempfile
from test_firefly import execute, success


PRELOAD = r"""
const {contextBridge}=require('MODULE');
globalThis.privatePreloadValue='hidden';
globalThis.preloadRuns=(globalThis.preloadRuns||0)+1;
contextBridge.exposeInMainWorld('desktop',{
 version:VERSION,boot:preloadRuns,data:{frozen:true},
 nested:{sum:(a,b)=>a+b},later:async()=>({value:VERSION+1})
});
"""
PAGE = r"""<!doctype html><meta charset="utf-8"><title>LOADING</title>
<script>
const early=globalThis.desktop;
const fail=()=>document.title='PRELOAD_FAILURE';
if(!early || early.boot!==1 || !Object.isFrozen(early) || !Object.isFrozen(early.data) || early.nested.sum(20,22)!==42 ||
   typeof privatePreloadValue!=='undefined' || typeof require!=='undefined' || typeof process!=='undefined' || typeof Buffer!=='undefined' ||
   typeof __fireflyFunctionProbe!=='undefined' || typeof __fireflyDataProbe!=='undefined' || typeof __fireflyContextApiProbe!=='undefined')fail();
window.onload=async()=>{
 try{
  if(document.title==='PRELOAD_FAILURE')return;
  const result=await early.later();if(result.value!==early.version+1)throw Error('Promise result');
  const frame=document.createElement('iframe');
  await new Promise(resolve=>{frame.onload=resolve;frame.srcdoc='<!doctype html><title>frame</title>';document.body.append(frame)});
  if(typeof frame.contentWindow.desktop!=='undefined' || typeof frame.contentWindow.require!=='undefined')throw Error('Subframe capability');
  frame.remove();document.title='PUBLIC_PRELOAD_'+early.version;
 }catch(error){fail()}
};
</script><body>Public isolated preload test</body>"""
MAIN = r"""
const {app,BrowserWindow}=require(process.env.FIREFLY_API_MODULE||'firefly');
const path=require('node:path'),fs=require('node:fs'),http=require('node:http');
async function title(win,expected){
 for(let i=0;i<300;i++){
  const value=win.webContents.getTitle();if(value===expected)return;
  if(value==='PRELOAD_FAILURE')throw Error('Preload page assertions failed');
  await new Promise(resolve=>setTimeout(resolve,10));
 }
 throw Error('Missing preload title '+expected+': '+win.webContents.getTitle());
}
app.whenReady().then(async()=>{
 const a=new BrowserWindow({show:false,webPreferences:{preload:path.join(__dirname,'preload.cjs')}});
 const b=new BrowserWindow({show:false,webPreferences:{preload:path.join(__dirname,'second.cjs')}});
 const plain=new BrowserWindow({show:false});
 await Promise.all([a.loadFile('page.html'),b.loadFile('page.html'),plain.loadFile('plain.html')]);
 await Promise.all([title(a,'PUBLIC_PRELOAD_42'),title(b,'PUBLIC_PRELOAD_7'),title(plain,'NO_PRELOAD')]);
 await a.loadFile('again.html');await title(a,'PUBLIC_PRELOAD_42');
 const body=fs.readFileSync(path.join(__dirname,'page.html'));
 const server=http.createServer((_req,res)=>{res.setHeader('Connection','close');res.setHeader('Content-Type','text/html');res.end(body)});
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 await a.loadURL('http://127.0.0.1:'+server.address().port+'/');await title(a,'PUBLIC_PRELOAD_42');
 await new Promise(resolve=>server.close(resolve));
 console.log(JSON.stringify({preload:true,isolated:true,windows:3,reload:true,http:true}));
 for(const win of [a,b,plain])win.destroy();app.quit();
}).catch(error=>{console.error(error.message);app.exit(1)});
"""


def firefly_errors(binary, root):
    """Firefly-specific explicit limits and fail-closed behavior, not Electron parity."""
    (root / "large.cjs").write_bytes(b" " * 65537)
    (root / "invalid.cjs").write_bytes(b"\xff")
    cases = [
        ("42", "ERR_INVALID_ARG_TYPE"),
        ("'relative.cjs'", "ERR_INVALID_ARG_VALUE"),
        ("path.join(__dirname,'missing.cjs')", "ERR_FIREFLY_PRELOAD_READ_FAILED"),
        ("__dirname", "ERR_FIREFLY_PRELOAD_READ_FAILED"),
        ("path.join(__dirname,'large.cjs')", "ERR_FIREFLY_PRELOAD_TOO_LARGE"),
        ("path.join(__dirname,'invalid.cjs')", "ERR_FIREFLY_PRELOAD_READ_FAILED"),
        ("path.join(__dirname,'esm.mjs')", "ERR_FIREFLY_UNSUPPORTED_API"),
        ("__dirname+'\\0bad.cjs'", "ERR_INVALID_ARG_VALUE"),
    ]
    assertions = "".join(
        "try{new BrowserWindow({show:false,webPreferences:{preload:"
        + value
        + "}});throw Error('Accepted invalid preload')}catch(e){if(e.code!=="
        + json.dumps(code)
        + ")throw e;}\n"
        for value, code in cases
    )
    (root / "errors.cjs").write_text(
        "const {app,BrowserWindow}=require('firefly'),path=require('node:path');app.whenReady().then(()=>{"
        + assertions
        + "console.log(JSON.stringify({errors:8}));app.quit()}).catch(e=>{console.error(e.message);app.exit(1)});",
        encoding="utf-8",
    )
    assert success(execute(binary, root / "errors.cjs")) == {"errors": 8}
    # The size limit includes exactly 64 KiB; unknown loaders reject without granting FS.
    source = """
const {contextBridge}=require('firefly');
let denied=false;try{require('node:fs')}catch(e){denied=e.code==='ERR_FIREFLY_UNSUPPORTED_API'}
contextBridge.exposeInMainWorld('checked',{
 denied,receiver:this===module.exports,args:arguments.length,
 metadata:__filename.endsWith('boundary.cjs')&&__dirname.length>0,
 clean:process.sandboxed===true&&process.type==='renderer'&&typeof process.binding==='undefined'&&typeof Buffer==='undefined',
 getter(value){return value.secret}
});
"""
    (root / "boundary.cjs").write_bytes(source.encode() + b" " * (65536 - len(source.encode())))
    (root / "boundary.html").write_text(
        """<!doctype html><title>FAIL</title><script>
let traps=0;const x={get secret(){traps++;return 42}};let rejected=false;
try{checked.getter(x)}catch(e){rejected=true}
if(checked.denied&&checked.receiver&&checked.args===5&&checked.metadata&&checked.clean&&rejected&&traps===0)document.title='CHECKED';
</script>""",
        encoding="utf-8",
    )
    (root / "boundary-main.cjs").write_text(
        """
const {app,BrowserWindow}=require('firefly'),path=require('node:path'),fs=require('node:fs');
app.whenReady().then(async()=>{const w=new BrowserWindow({show:false,webPreferences:{preload:path.join(__dirname,'boundary.cjs')}});
await w.loadFile('boundary.html');if(w.webContents.getTitle()!=='CHECKED')throw Error('Boundary assertions failed');
fs.writeFileSync(path.join(__dirname,'boundary.cjs'),"throw Error('Must reuse construction snapshot')");
await w.loadFile('boundary.html');if(w.webContents.getTitle()!=='CHECKED')throw Error('Snapshot changed');
console.log(JSON.stringify({boundary:true}));w.destroy();app.quit()}).catch(e=>{console.error(e.message);app.exit(1)});
""",
        encoding="utf-8",
    )
    assert success(execute(binary, root / "boundary-main.cjs")) == {"boundary": True}
    for bad in ["const = ;", "throw Error('private failure detail')"]:
        (root / "bad.cjs").write_text(bad, encoding="utf-8")
        (root / "bad-main.cjs").write_text(
            """
const {app,BrowserWindow}=require('firefly'),path=require('node:path');
app.whenReady().then(async()=>{const w=new BrowserWindow({show:false,webPreferences:{preload:path.join(__dirname,'bad.cjs')}});
try{await w.loadFile('plain.html');throw Error('Failed preload loaded successfully')}catch(e){if(e.code!=='ERR_FAILED')throw e;}
console.log(JSON.stringify({failedClosed:true}));w.destroy();app.quit()}).catch(e=>{console.error(e.message);app.exit(1)});
""",
            encoding="utf-8",
        )
        result = execute(binary, root / "bad-main.cjs")
        assert success(result) == {"failedClosed": True}, result
        assert "private failure detail" not in result.stderr, result
    print(
        "PASS Firefly preload validation, exact 64 KiB boundary, restricted CommonJS, getter rejection and syntax/throw fail-closed loads"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument("--electron", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-public-preload-") as directory:
        root = Path(directory).resolve() / "app é文🪲"
        root.mkdir()
        (root / "main.cjs").write_text(MAIN, encoding="utf-8")
        (root / "page.html").write_text(PAGE, encoding="utf-8")
        (root / "again.html").write_text(PAGE, encoding="utf-8")
        (root / "plain.html").write_text(
            "<!doctype html><title>NO_PRELOAD</title><script>if(typeof desktop!=='undefined'||typeof require!=='undefined'||typeof __fireflyFunctionProbe!=='undefined')document.title='PRELOAD_FAILURE';</script>",
            encoding="utf-8",
        )
        observations = []
        for binary, module in [(args.electron, "electron"), (args.firefly, "firefly")]:
            for name, version in [("preload.cjs", 42), ("second.cjs", 7)]:
                (root / name).write_text(
                    PRELOAD.replace("MODULE", module).replace("VERSION", str(version)),
                    encoding="utf-8",
                )
            result = execute(binary.resolve(), root / "main.cjs", module)
            observations.append(success(result))
        assert (
            observations[0]
            == observations[1]
            == {"preload": True, "isolated": True, "windows": 3, "reload": True, "http": True}
        ), observations
        firefly_errors(args.firefly.resolve(), root)
    print(
        "PASS shared Electron/Firefly public preload: first script, nested frozen API/Promise, three windows, no subframe installation, reload and file-to-HTTP process replacement"
    )


if __name__ == "__main__":
    main()
