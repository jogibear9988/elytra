#!/usr/bin/env python3
"""Route only the generated traversal filter getter return through our policy."""
import argparse
import json
from pathlib import Path

EDIT=('    return R;', '    return Firefly::Callbacks::read_filter(realm, idl_object->filter().ptr(), R);')
def adapt(text,interface):
    marker=f'JS_DEFINE_NATIVE_FUNCTION({interface}Prototype::filter_getter)\n'
    if text.count(marker)!=1:raise ValueError('Review generated traversal filter getter')
    start=text.index(marker);end=text.index('\n}\n',start)+3
    before,after=EDIT
    block=text[start:end]
    if block.count(before)!=1:raise ValueError('Review generated traversal filter return')
    return text[:start]+block.replace(before,after)+text[end:]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--interface',required=True,choices=['NodeIterator','TreeWalker'])
    args=parser.parse_args();source=args.source.resolve()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(('// Generated traversal filter adapter; do not edit.\n#line 1 '+json.dumps(str(source))+'\n'+adapt(source.read_bytes().decode('utf-8'),args.interface)).encode('utf-8'))
if __name__=='__main__':main()
