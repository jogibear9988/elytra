#!/usr/bin/env python3
"""Reject an upstream update that changes the narrow renderer entry adapters."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
HOOKS={
    'Services/WebContent/main.cpp':['ladybird_main','initialize_main_thread_vm'],
    'Services/WebContent/WebDriverBiDi.cpp':['set_webdriver_bidi_session_state'],
}


def verify_text(text, names):
    for name in names:
        if len(re.findall(r'\b'+re.escape(name)+r'\b',text))!=1:
            raise ValueError(f'Review renderer adapter for changed entry point: {name}')


def main():
    for relative,names in HOOKS.items():
        verify_text((ROOT/'vendor/ladybird'/relative).read_text(),names)
    print('Narrow renderer entry adapters verified')


if __name__=='__main__':main()
