#!/usr/bin/env python3
"""Check the actual sandboxed helper before page scripts, with an unchanged control."""
import argparse
import re
from pathlib import Path
from test_renderer import ROOT,run
from check_renderer_hooks import EXPECTED_COUNTS,HOOKS,verify_text


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
    errors=run(probe,'--headless=manual',(fixture/'errors.html').as_uri())
    pid=pid_of(errors)
    rejections=[int(n) for n in re.findall(rf'FIREFLY_WORLD_FAILURES_REDACTED pid={pid} rejections=(\d+)',errors.stderr)]
    assert rejections and max(rejections)>=4,errors
    timers=run(probe,'--headless=manual',(fixture/'timers.html').as_uri())
    pid=pid_of(timers)
    canceled=[int(n) for n in re.findall(rf'FIREFLY_WORLD_TIMERS_CANCELED pid={pid} count=(\d+)',timers.stderr)]
    assert canceled and max(canceled)>=2,timers
    observers=run(probe,'--headless=manual',(fixture/'observers.html').as_uri())
    pid=pid_of(observers)
    canceled=[int(n) for n in re.findall(rf'FIREFLY_WORLD_OBSERVERS_CANCELED pid={pid} count=(\d+)',observers.stderr)]
    assert canceled and max(canceled)>=1,observers
    listeners=run(probe,'--headless=manual',(fixture/'listeners.html').as_uri())
    pid=pid_of(listeners)
    failures=[int(n) for n in re.findall(rf'FIREFLY_WORLD_FAILURES_REDACTED pid={pid} rejections=\d+ job_errors=(\d+)',listeners.stderr)]
    assert failures and max(failures)>=2,listeners
    canceled=[(int(n),int(s)) for n,s in re.findall(rf'FIREFLY_WORLD_LISTENERS_CANCELED pid={pid} count=(\d+) signals=(\d+)',listeners.stderr)]
    assert canceled and any(n>=4 and s>=2 for n,s in canceled),listeners
    handlers=run(probe,'--headless=manual',(fixture/'handlers.html').as_uri())
    pid=pid_of(handlers)
    failures=[int(n) for n in re.findall(rf'FIREFLY_WORLD_FAILURES_REDACTED pid={pid} rejections=\d+ job_errors=(\d+)',handlers.stderr)]
    assert failures and max(failures)>=1,handlers
    maps=[int(n) for n in re.findall(rf'FIREFLY_WORLD_HANDLER_MAPS_CLEARED pid={pid} count=(\d+)',handlers.stderr)]
    assert maps and max(maps)>=2,handlers
    custom=run(probe,'--headless=manual',(fixture/'custom-events.html').as_uri())
    pid_of(custom)
    error_values=run(probe,'--headless=manual',(fixture/'error-events.html').as_uri())
    pid_of(error_values)
    messages=run(probe,'--headless=manual',(fixture/'message-events.html').as_uri())
    pid_of(messages)
    filters=run(probe,'--headless=manual',(fixture/'node-filters.html').as_uri())
    pid_of(filters)
    principal=run(probe,'--headless=manual',(fixture/'principal-callbacks.html').as_uri())
    pid=pid_of(principal)
    canceled=[int(n) for n in re.findall(rf'FIREFLY_WORLD_TIMERS_CANCELED pid={pid} count=(\d+)',principal.stderr)]
    assert canceled and max(canceled)>=1,principal
    # A lifecycle-only manual run must still fail the process if page ordering or
    # isolation fails, so the fixture additionally checks it before closing.
    for name in {name for names in HOOKS.values() for name in names}:
        expected=EXPECTED_COUNTS.get(name,1)
        verify_text(' '.join([name]*expected),[name])
        for changed in ['', ' '.join([name]*(expected+1))]:
            try:verify_text(changed,[name])
            except ValueError:pass
            else:raise AssertionError('Changed entry adapter accepted')
    for relative,names in HOOKS.items():verify_text((ROOT/'vendor/ladybird'/relative).read_text(),names)
    print('PASS upstream helper without registered worlds lacks early world; owned helper runs before author script')
    print('PASS isolated globals and frozen data-only copies in top-level/srcdoc frames and matching renderer PID')
    print('PASS function calls copy data and reject removed-frame references across 70 cleanup cycles')
    print('PASS native Promise results and removed-frame pending rejection')
    print('PASS removed-frame Promise/WebIDL microtasks blocked before execution')
    print('PASS running reaction/await/generator/queueMicrotask callbacks remove frames and reject safely')
    print('PASS source rejection objects/functions stay private; page reject/handle events remain')
    print('PASS source interval/timeout native cancellation leaves the document and page interval live')
    print('PASS source observer wrappers resist page prototypes; native disconnect leaves page observers live')
    print('PASS source listener lifetime/error privacy and native registration/signal release; page listener/abort continues')
    print('PASS source replacement handler return/cancelation/error/lifetime and continuing page controls')
    print('PASS CustomEvent snapshots/cache/identity, rejected raw functions and page getters, source revoke and fresh-reader denial')
    print('PASS ErrorEvent property/onerror copies, rejected raw functions/page getters and preserved native page reporting')
    print('PASS MessageEvent snapshots/init metadata and actual two-way postMessage serialization/delivery after source detach')
    print('PASS principal listener/handler/reporter frames and jobs/timers stay independent of source dispatch/revoke')
    print('PASS native NodeIterator/TreeWalker callback worlds, private errors and frame removal during return conversion')
    print('PASS changed entry points rejected; default sandbox and physical resource staging retained')


if __name__=='__main__':main()
