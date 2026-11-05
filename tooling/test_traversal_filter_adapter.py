#!/usr/bin/env python3
"""Check generated filter getter preservation and drift rejection."""
import argparse
from pathlib import Path
from adapt_traversal_filter import EDIT,adapt
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bindings',required=True,type=Path)
args=parser.parse_args()
for interface in ['NodeIterator','TreeWalker']:
    original=(args.bindings/(interface+'.cpp')).read_bytes().decode('utf-8')
    result=adapt(original,interface)
    assert result.replace(EDIT[1],EDIT[0])==original
    marker=f'JS_DEFINE_NATIVE_FUNCTION({interface}Prototype::filter_getter)\n'
    for invalid in [original.replace(marker,'changed-getter\n'),original+'\n'+marker,result.replace(EDIT[1],'changed-return')]:
        try:adapt(invalid,interface)
        except ValueError:pass
        else:raise AssertionError('Generated getter drift accepted')
    extra='\nJS::Value unrelated() {\n    return R;\n}\n'
    assert adapt(original+extra,interface).endswith(extra)
print('PASS traversal filter getter source preservation and missing/duplicate/return drift rejection')
