#!/usr/bin/env python3
"""Check definition-only adaptation, byte preservation and upstream drift rejection."""
from pathlib import Path
from adapt_node_filter import ENTRY, REPLACEMENT, adapt

original=(Path(__file__).resolve().parents[1]/'vendor/ladybird/Libraries/LibWeb/DOM/NodeFilter.cpp').read_bytes().decode('utf-8')
result=adapt(original)
assert result.replace(REPLACEMENT,ENTRY)==original
assert 'auto result = accept_node(realm, *traverser.filter(), node);' in result
for changed in (original.replace(ENTRY,'changed('), original+'\n'+ENTRY):
    try: adapt(changed)
    except ValueError: pass
    else: raise AssertionError('Missing or duplicate entry was accepted')
print('PASS NodeFilter adapter preserves all bytes outside one definition and rejects entry drift')
