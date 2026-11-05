#!/usr/bin/env python3
"""Require exact CustomEvent seams and byte preservation outside the declared edits."""
import argparse
from pathlib import Path
from adapt_custom_event import NATIVE_EDITS,BINDING_EDIT,adapt

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native',required=True,type=Path)
parser.add_argument('--binding',required=True,type=Path)
args=parser.parse_args()
for kind,path,edits in [('native',args.native,NATIVE_EDITS),('binding',args.binding,(BINDING_EDIT,))]:
    original=path.read_bytes().decode('utf-8');modified=adapt(original,kind);restored=modified
    for before,after in reversed(edits):
        assert restored.count(after)==1,(kind,after)
        restored=restored.replace(after,before)
        missing=original.replace(before,'changed-value-seam',1)
        try:adapt(missing,kind)
        except (ValueError,IndexError):pass
        else:raise AssertionError('Missing value seam accepted')
    assert restored==original,'Unrelated CustomEvent source changed'
    if kind=='native':
        for before,after in edits:
            try:adapt(original+'\n'+before,kind)
            except ValueError:pass
            else:raise AssertionError('Duplicate native seam accepted')
    else:
        marker='JS_DEFINE_NATIVE_FUNCTION(CustomEventPrototype::detail_getter)\n'
        try:adapt(original+'\n'+marker,kind)
        except ValueError:pass
        else:raise AssertionError('Duplicate getter accepted')
        extra='\nJS::Value unrelated() {\n    return R;\n}\n'
        assert adapt(original+extra,kind).endswith(extra),'Unrelated return rewritten'
print('PASS native/generated CustomEvent seam drift rejected; all other source bytes preserved')
