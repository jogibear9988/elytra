#!/usr/bin/env python3
"""Exercise actual native loopback TCP with both Node and Mite."""
import argparse
from pathlib import Path
import shutil
import socket
import threading
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node: parser.error('Node is required for differential tests')
    root=Path(__file__).resolve().parents[1]/'mite/tests/net'
    failures=0
    for name in ['echo.cjs','half-open.mjs','backpressure.mjs','close-wait.mjs','refused.mjs','ipv6.mjs','unref.cjs','buffered.mjs','timeout.mjs','listen-errors.mjs','cancel.cjs','unref-client.mjs','checkpoint.mjs','cancel-write.mjs']:
        try:
            results=[subprocess.run([exe,str(root/name)],capture_output=True,text=True,timeout=20) for exe in [args.node,args.mite]]
            passed=all(r.returncode==0 and not r.stderr for r in results) and results[0].stdout==results[1].stdout
        except subprocess.TimeoutExpired as e:
            print(f'FAIL {name}: timed out: {e} {e.stdout!r} {e.stderr!r}');failures+=1;continue
        print(f'{"PASS" if passed else "FAIL"} net/{name}')
        if not passed:
            failures+=1;print('Node:',results[0]);print('Mite:',results[1])
    for exe in [args.node,args.mite]:
        # An independent OS socket peer rules out two identical runtime bugs
        # masking each other in the loopback echo workflows.
        process=subprocess.Popen([exe,str(root/'external.cjs')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        watchdog=threading.Timer(15,process.kill);watchdog.start()
        passed=False
        try:
            port=int(process.stdout.readline())
            payload=bytes(index%251 for index in range(262173))
            with socket.create_connection(('127.0.0.1',port),timeout=5) as peer:
                peer.sendall(payload);peer.shutdown(socket.SHUT_WR)
                reply=bytearray()
                while True:
                    data=peer.recv(65536)
                    if not data: break
                    reply.extend(data)
            stdout,stderr=process.communicate(timeout=5)
            passed=reply==payload and process.returncode==0 and not stdout and not stderr
        except (OSError,ValueError,subprocess.TimeoutExpired) as e: print(e)
        finally:
            watchdog.cancel()
            if process.poll() is None: process.kill()
            process.communicate()
        print(f'{"PASS" if passed else "FAIL"} independent TCP peer ({Path(exe).name})')
        failures+=int(not passed)
    for exe in [args.node,args.mite]:
        try:
            result=subprocess.run([exe,str(root/'fatal.cjs')],capture_output=True,text=True,timeout=10)
            passed=result.returncode==1 and 'native socket callback failed' in result.stderr
        except subprocess.TimeoutExpired: passed=False
        print(f'{"PASS" if passed else "FAIL"} fatal TCP callback exits ({Path(exe).name})')
        failures+=int(not passed)
    result=subprocess.run([args.mite,str(root/'unsupported.cjs')],capture_output=True,text=True,timeout=10)
    passed=result.returncode==0 and not result.stderr and result.stdout=='ERR_MITE_UNSUPPORTED_API\n'*9
    print(f'{"PASS" if passed else "FAIL"} explicit unsupported socket APIs')
    if not passed: print(result)
    failures+=int(not passed)
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
