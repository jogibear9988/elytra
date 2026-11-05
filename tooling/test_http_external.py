#!/usr/bin/env python3
"""Validate an HTTP server with the independent Python standard-library client."""
import argparse
import http.client
import json
from pathlib import Path
import shutil
import subprocess
import threading
from email.utils import parsedate_to_datetime


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node:parser.error('Node is required')
    fixture=Path(__file__).resolve().parents[1]/'mite/tests/http/external.mjs'
    failures=0
    for exe in [args.node,args.mite]:
        process=subprocess.Popen([exe,str(fixture)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        watchdog=threading.Timer(20,process.kill);watchdog.start()
        passed=False;connection=None
        try:
            port=int(process.stdout.readline())
            connection=http.client.HTTPConnection('127.0.0.1',port,timeout=5)
            connection.request('GET','/hello?q=1',headers={'User-Agent':'independent-client'})
            response=connection.getresponse()
            assert response.status==200 and response.getheader('content-type')=='application/json'
            assert parsedate_to_datetime(response.getheader('date')).tzinfo is not None
            assert json.loads(response.read())=={'method':'GET','path':'/hello?q=1','agent':'independent-client'}
            payload=bytes(i%251 for i in range(262173))
            connection.request('POST','/echo',payload)
            response=connection.getresponse()
            assert response.status==202 and response.getheader('transfer-encoding')=='chunked'
            assert response.read()==payload
            connection.request('GET','/stream');response=connection.getresponse()
            assert response.status==200 and response.read()==b'firstsecond'
            connection.request('HEAD','/head');response=connection.getresponse()
            assert response.status==200 and response.read()==b''
            connection.request('GET','/empty');response=connection.getresponse()
            assert response.status==204 and response.read()==b''
            connection.request('GET','/done');response=connection.getresponse()
            assert response.status==200 and response.read()==b'done'
            stdout,stderr=process.communicate(timeout=5)
            assert process.returncode==0 and not stderr and stdout=='closed 1 6\n',(process.returncode,stdout,stderr)
            passed=True
        except (OSError,ValueError,AssertionError,subprocess.TimeoutExpired,http.client.HTTPException) as e:print(repr(e))
        finally:
            watchdog.cancel()
            if connection:connection.close()
            if process.poll() is None:process.kill()
            process.communicate()
        print(f'{"PASS" if passed else "FAIL"} six independent HTTP-client requests ({Path(exe).name})')
        failures+=int(not passed)
    return int(bool(failures))


if __name__=='__main__':raise SystemExit(main())
