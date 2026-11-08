#!/usr/bin/env python3
"""Real native input, hidden Qt chooser and owned helper File API acceptance."""

import _support
import argparse
from pathlib import Path
import tempfile
from test_renderer import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-file-input-") as directory:
        root = Path(directory)
        selected = root / "ä selected.txt"
        prefix = "native selected ✓".encode()
        selected.write_bytes(prefix + b"\0" + b"x" * (4 * 1024 * 1024 - len(prefix) - 1))
        (root / "page.html").write_text(
            """<!doctype html><meta charset="utf-8"><title>File input</title>
<input type="file" accept=".txt" style="position:absolute;left:20px;top:20px;width:240px;height:50px">
<script>
const input=document.querySelector('input');
input.onchange=async()=>{try{
const file=input.files[0];
const text=await file.text(), data=new Uint8Array(await file.arrayBuffer());
if(input.files.length!==1||file.name!=='ä selected.txt'||file.size!==4*1024*1024)throw Error('file metadata');
if(!text.startsWith('native selected ✓\\0')||!text.endsWith('x')||text.length!==file.size-2)throw Error('file text');
if(data.length!==file.size||data[0]!==110||data[19]!==0)throw Error('file binary prefix');
for(let index=20;index<data.length;index++)if(data[index]!==120)throw Error('file binary body');
if(input.value.includes('firefly-file-input-')||file.name.includes('/'))throw Error('path disclosure');
document.title='PASS_SELECTED';
}catch(e){document.title='FAIL:'+e}};
input.oncancel=()=>document.title='PASS_CANCELLED';
</script>""",
            encoding="utf-8",
        )
        following = root / "new.html"
        following.write_text("""<!doctype html><title>New document</title><input type="file">
<script>document.title=document.querySelector('input').files.length===0?'PASS_NEW_DOCUMENT':'FAIL:stale files'</script>""")
        result = run(
            args.probe.resolve(),
            (root / "page.html").as_uri(),
            "--probe-file",
            selected,
            "--probe-next",
            following.as_uri(),
        )
        assert result.returncode == 0 and "PASS hidden native file input:" in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
