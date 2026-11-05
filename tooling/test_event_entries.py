#!/usr/bin/env python3
"""Verify entry adaptation preserves every byte except the two definition names."""
import difflib
from pathlib import Path
from adapt_event_entries import ENTRIES,adapt

source=Path(__file__).resolve().parents[1]/'vendor/ladybird/Libraries/LibWeb/DOM/EventTarget.cpp'
original=source.read_bytes().decode('utf-8')
modified=adapt(original)
changes=[line for line in difflib.ndiff(original.splitlines(),modified.splitlines()) if line.startswith(('- ','+ '))]
assert len(changes)==4,changes
for name in ENTRIES:
    before=[line[2:] for line in changes if line.startswith('- JS::Completion '+name+'(')]
    after=[line[2:] for line in changes if line.startswith('+ JS::Completion firefly_original_'+name+'(')]
    assert len(before)==len(after)==1 and before[0].replace(name,'firefly_original_'+name,1)==after[0],changes
    for invalid in [original.replace('JS::Completion '+name+'(', 'JS::Completion changed_'+name+'(',1),original+'\nJS::Completion '+name+'(\n']:
        try:adapt(invalid)
        except ValueError:pass
        else:raise AssertionError('Changed/duplicate definition accepted')
assert 'Bindings::invoke_event_handler(*callback, event, special_error_event_handling)' in modified
print('PASS generated adapter changes only two definition names; bodies, calls, licenses and other bytes preserved')
