#!/usr/bin/env python3
"""Rename only free entry definitions; preserve internal calls to guarded entries."""
import argparse
import json
import re
from pathlib import Path

ENTRIES=('invoke_event_listener','invoke_event_handler')


def adapt(text):
    for name in ENTRIES:
        pattern=re.compile(r'^JS::Completion '+name+r'(?=\()',re.MULTILINE)
        text,count=pattern.subn('JS::Completion firefly_original_'+name,text)
        if count!=1:
            raise ValueError(f'Expected exactly one {name} definition; review upstream entry')
    return text


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    source=args.source.resolve()
    original=source.read_bytes().decode('utf-8')
    generated='// Generated entry-name adapter; do not edit.\n#line 1 '+json.dumps(str(source),ensure_ascii=False)+'\n'+adapt(original)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(generated.encode('utf-8'))


if __name__=='__main__':main()
