#!/usr/bin/env python3
"""Adapt native entries and handler-map addresses without replacing algorithms."""
import argparse
import json
import re
from pathlib import Path

# Exact address/scope seams, not copies of the event-handler algorithms.
STORAGE_EDITS=(
    ('error_event.error()', 'TRY(Firefly::EventValues::read(callback.associated_realm(), error_event, error_event.error()))', 1),
    ('JS::Realm& realm = event_handler_listener_realm(*this, event_handler);', 'JS::Realm& realm = Firefly::Handlers::listener_realm(*this, event_handler_listener_realm(*this, event_handler));', 1),
    ('auto& handler_map = ensure_data().event_handler_map;', 'auto& handler_map = Firefly::Handlers::select_map(*this, ensure_data().event_handler_map);', 2),
    ('auto& handler_map = event_target->ensure_data().event_handler_map;', 'auto& handler_map = Firefly::Handlers::select_map(*event_target, event_target->ensure_data().event_handler_map);', 2),
    ('visitor.visit(data->event_handler_map);', 'visitor.visit(data->event_handler_map);\n        Firefly::Handlers::visit_maps(*this, visitor);', 1),
    ('void EventTarget::element_event_handler_attribute_changed(Utf16FlyString const& local_name, Optional<Utf16String> const& value)\n{', 'void EventTarget::element_event_handler_attribute_changed(Utf16FlyString const& local_name, Optional<Utf16String> const& value)\n{\n    Firefly::Handlers::PrincipalScope firefly_principal_scope(vm());', 1),
    ('void EventTarget::erase_all_event_listeners_and_handlers()\n{', 'void EventTarget::erase_all_event_listeners_and_handlers()\n{\n    Firefly::Handlers::PrincipalScope firefly_principal_scope(vm());\n    Firefly::Handlers::clear_maps(*this);', 1),
)
ENTRIES=('invoke_event_listener','invoke_event_handler')


def adapt(text):
    for name in ENTRIES:
        pattern=re.compile(r'^JS::Completion '+name+r'(?=\()',re.MULTILINE)
        text,count=pattern.subn('JS::Completion firefly_original_'+name,text)
        if count!=1:
            raise ValueError(f'Expected exactly one {name} definition; review upstream entry')
    for original,replacement,expected in STORAGE_EDITS:
        if text.count(original)!=expected:
            raise ValueError('Review upstream handler storage seam: '+original)
        text=text.replace(original,replacement)
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
