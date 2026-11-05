#!/usr/bin/env python3
"""Check the actual sandboxed helper before page scripts, with an unchanged control."""
import argparse
import re
from pathlib import Path
from test_renderer import ROOT,run
from check_renderer_hooks import HOOKS,verify_text


def pid_of(result):
    match=re.search(r'^WebContent PID: (\d+)$',result.stdout,re.MULTILINE)
    assert result.returncode==0 and match,result
    pid=int(match[1]);assert pid>0 and pid!=result.pid,result
    assert 'PathFontProvider::load_all_fonts' not in result.stderr,result
    return pid


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',required=True,type=Path)
    parser.add_argument('--control',required=True,type=Path)
    args=parser.parse_args()
    probe=args.probe.resolve();control=args.control.resolve()
    if not probe.is_file() or not control.is_file():parser.error('Build both renderer probes first')
    fixture=ROOT/'firefly/tests/preload'
    negative=run(control,'--headless=text',(fixture/'first-script.html').as_uri())
    pid_of(negative)
    assert 'BEFORE_SCRIPT_MISSING' in negative.stdout and 'FIREFLY_WORLD_CREATED' not in negative.stderr,negative
    first=run(probe,'--headless=text',(fixture/'first-script.html').as_uri())
    pid=pid_of(first)
    assert 'BEFORE_SCRIPT_OK' in first.stdout and f'FIREFLY_WORLD_CREATED pid={pid} ' in first.stderr,first
    frame=run(probe,'--headless=text',(fixture/'frame.html').as_uri())
    pid_of(frame);assert 'FRAME_WORLD_OK' in frame.stdout,frame
    lifecycle=run(probe,'--headless=manual',(fixture/'frame.html').as_uri())
    pid=pid_of(lifecycle)
    created=[int(n) for n in re.findall(rf'FIREFLY_WORLD_CREATED pid={pid} count=(\d+)',lifecycle.stderr)]
    destroyed=[int(n) for n in re.findall(rf'FIREFLY_WORLD_DESTROYED pid={pid} count=(\d+)',lifecycle.stderr)]
    assert created and max(created)>=3 and destroyed and max(destroyed)>=2,lifecycle
    cycles=run(probe,'--headless=manual',(fixture/'cycles.html').as_uri())
    pid=pid_of(cycles)
    created=[int(n) for n in re.findall(rf'FIREFLY_WORLD_CREATED pid={pid} count=(\d+)',cycles.stderr)]
    destroyed=[int(n) for n in re.findall(rf'FIREFLY_WORLD_DESTROYED pid={pid} count=(\d+)',cycles.stderr)]
    assert created and max(created)>=141 and destroyed and max(destroyed)>=140,cycles
    promises=run(probe,'--headless=manual',(fixture/'promises.html').as_uri())
    pid_of(promises)
    # The promise fixture closes only after fulfillment copies, sanitized
    # rejection and a retained removed-frame promise rejection all succeed.
    jobs=run(probe,'--headless=manual',(fixture/'jobs.html').as_uri())
    pid=pid_of(jobs)
    blocked=[int(n) for n in re.findall(rf'FIREFLY_WORLD_JOBS_BLOCKED pid={pid} count=(\d+)',jobs.stderr)]
    assert blocked and max(blocked)>=6,jobs
    active=run(probe,'--headless=manual',(fixture/'async-frames.html').as_uri())
    pid_of(active)
    # A lifecycle-only manual run must still fail the process if page ordering or
    # isolation fails, so the fixture additionally checks it before closing.
    for name in ['ladybird_main','initialize_main_thread_vm','set_webdriver_bidi_session_state']:
        for changed in ['',name+' '+name]:
            try:verify_text(changed,[name])
            except ValueError:pass
            else:raise AssertionError('Changed entry adapter accepted')
    for relative,names in HOOKS.items():verify_text((ROOT/'vendor/ladybird'/relative).read_text(),names)
    print('PASS unmodified control lacks early world, owned helper creates it before first author script')
    print('PASS isolated globals and frozen data-only copies in top-level/srcdoc frames and matching renderer PID')
    print('PASS function calls copy data and reject removed-frame references across 70 cleanup cycles')
    print('PASS native Promise results and removed-frame pending rejection')
    print('PASS removed-frame reaction/thenable/async jobs blocked before execution')
    print('PASS running reaction/await/generator jobs remove their own frames and reject target promises safely')
    print('PASS changed entry points rejected; default sandbox and physical resource staging retained')


if __name__=='__main__':main()
