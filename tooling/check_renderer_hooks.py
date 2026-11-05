#!/usr/bin/env python3
"""Reject an upstream update that changes the narrow renderer entry adapters."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
HOOKS={
    'Services/WebContent/main.cpp':['ladybird_main','initialize_main_thread_vm'],
    'Services/WebContent/WebDriverBiDi.cpp':['set_webdriver_bidi_session_state'],
    'Libraries/LibWeb/WebIDL/AbstractOperations.cpp':['invoke_callback'],
    'Libraries/LibWeb/HTML/WindowOrWorkerGlobalScope.cpp':['m_timers'],
    'Libraries/LibWeb/DOM/EventTarget.cpp':['invoke_event_listener','invoke_event_handler','event_listener_list_changed','add_abort_algorithm'],
    'Libraries/LibWeb/DOM/MutationObserver.cpp':['m_node_list'],
}


EXPECTED_COUNTS={'invoke_callback':4,'m_timers':15,'m_node_list':3,'event_listener_list_changed':2,'invoke_event_handler':2}

def verify_text(text, names):
    for name in names:
        if len(re.findall(r'\b'+re.escape(name)+r'\b',text))!=EXPECTED_COUNTS.get(name,1):
            raise ValueError(f'Review renderer adapter for changed entry point: {name}')


def main():
    for relative,names in HOOKS.items():
        verify_text((ROOT/'vendor/ladybird'/relative).read_text(),names)
    print('Narrow renderer entry adapters verified')


if __name__=='__main__':main()
