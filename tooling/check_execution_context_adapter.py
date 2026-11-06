#!/usr/bin/env python3
"""Fail on upstream drift of the one null-deallocation call we intercept."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DELETE = '''void ExecutionContext::operator delete(void* execution_context)
{
    js_execution_context_destroy(static_cast<JSExecutionContext*>(execution_context));
}'''


def check(source=None):
    if source is None:
        source = (ROOT / 'vendor/ladybird/Libraries/LibJS/Runtime/ExecutionContext.cpp').read_text()
    if source.count(DELETE) != 1 or source.count('js_execution_context_destroy(') != 1:
        raise ValueError('ExecutionContext delete seam changed; review/remove the owned null guard')
    print('PASS exact upstream execution-context deallocation seam')


if __name__ == '__main__':
    check()
