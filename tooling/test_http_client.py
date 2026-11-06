#!/usr/bin/env python3
"""Exercise HTTP clients against an independent raw TCP peer, with Node as reference."""
import argparse
import json
from pathlib import Path
import shutil
import socketserver
import subprocess
import tempfile
import threading
import time


class Peer(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        try:
            line = self.rfile.readline(8192).decode('latin1').strip()
            if not line:
                return
            method, path, version = line.split(' ')
            headers = {}
            while True:
                line = self.rfile.readline(32768)
                if line == b'\r\n':
                    break
                if not line:
                    return
                name, value = line.decode('latin1').split(':', 1)
                headers[name.lower()] = value.strip()
            body = b''
            if headers.get('transfer-encoding') == 'chunked':
                while True:
                    size = int(self.rfile.readline(64).strip(), 16)
                    if not size:
                        while self.rfile.readline(8192) != b'\r\n':
                            pass
                        break
                    body += self.rfile.read(size)
                    assert self.rfile.read(2) == b'\r\n'
                    assert len(body) < 4 * 1024 * 1024
            elif 'content-length' in headers:
                size = int(headers['content-length'])
                assert size < 4 * 1024 * 1024
                body = self.rfile.read(size)
            if path == '/hang':
                time.sleep(.2)
                return
            if path == '/malformed':
                response = b'NOTHTTP\r\n\r\n'
            elif path == '/overflow':
                response = b'HTTP/1.1 200 OK\r\nX-Large: ' + b'a' * 4096 + b'\r\n\r\n'
            elif path == '/truncated':
                response = b'HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort'
            elif path == '/ambiguous':
                response = b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n'
            elif path == '/upgrade':
                response = b'HTTP/1.1 101 Switching Protocols\r\nConnection: upgrade\r\nUpgrade: test\r\n\r\n'
            elif path == '/chunked':
                response = b'HTTP/1.1 200 Fine\r\nTransfer-Encoding: chunked\r\nX-Test: one\r\nX-Test: two\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\n\r\n3\r\none\r\n3\r\ntwo\r\n0\r\nX-Trailer: done\r\n\r\n'
            elif path == '/eof':
                response = b'HTTP/1.0 200 EOF\r\n\r\nno length'
            elif path == '/head':
                assert method == 'HEAD'
                response = b'HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n'
            elif path == '/empty':
                response = b'HTTP/1.1 204 No Content\r\n\r\n'
            elif path == '/large':
                data = bytes(range(256)) * 8192
                self.request.sendall(f'HTTP/1.1 200 OK\r\nContent-Length: {len(data)}\r\n\r\n'.encode())
                for offset in range(0, len(data), 16384):
                    self.request.sendall(data[offset:offset + 16384])
                return
            else:
                data = json.dumps({'method': method, 'path': path, 'version': version,
                                   'body': body.decode(), 'auth': headers.get('authorization'),
                                   'custom': headers.get('x-custom'), 'host': bool(headers.get('host'))},
                                  separators=(',', ':')).encode()
                response = (b'HTTP/1.1 201 Created\r\nContent-Length: ' + str(len(data)).encode() + b'\r\n\r\n' + data)
                if path == '/info':
                    response = b'HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\nLink: </a>\r\n\r\n' + response
            # Force arbitrary status/header/body fragmentation independently of JS.
            step = 7 if path in ['/chunked', '/eof', '/info'] else len(response)
            for offset in range(0, len(response), step):
                self.request.sendall(response[offset:offset + step])
                if step == 7:
                    time.sleep(.001)
        except (BrokenPipeError, ConnectionResetError):
            pass  # Expected when the client rejects oversized or malformed input.


SOURCE = r'''
(async()=>{
const http=require('node:http'),{Readable}=require('node:stream'),{createHash}=require('node:crypto');
const base=BASE,rows=[];
const ns=await import('node:http');
if(ns.request!==http.request || ns.get!==http.get || ns.ClientRequest!==http.ClientRequest || ns.default!==http)throw Error('HTTP module identity');
async function exchange(path,options={},upload){
 const info=[];let socketEvents=0,finished=false;
 return await new Promise((resolve,reject)=>{
  const req=http.request(new URL(base+path),{agent:false,...options},async res=>{
   try{
    if(!(res instanceof http.IncomingMessage) || res.req!==req)throw Error('Response identity');
    const chunks=[];
    for await(const chunk of res)chunks.push(chunk);
    const text=Buffer.concat(chunks).toString();
    resolve({status:res.statusCode,message:res.statusMessage,version:res.httpVersion,body:path.startsWith('/echo')||path==='/info'?JSON.parse(text):text,
      complete:res.complete,aborted:res.aborted,headers:res.headers,distinct:res.headersDistinct,raw:res.rawHeaders,trailers:res.trailers,rawTrailers:res.rawTrailers,
      info,socketEvents,finished});
   }catch(e){reject(e)}
  });
  if(!(req instanceof http.ClientRequest) || !(req instanceof http.OutgoingMessage))throw Error('Request identity');
  req.on('socket',()=>socketEvents++);req.on('finish',()=>finished=true);
  req.on('error',reject);req.on('continue',()=>info.push('continue'));
  req.on('information',value=>info.push([value.statusCode,value.statusMessage,value.httpVersion,value.headers]));
  if(upload==='stream')Readable.from(['first','second']).pipe(req);
  else if(upload==='chunks'){req.write('one');req.end('two');}
  else req.end(upload);
 });
}
for(const [path,options,upload] of [
 ['/echo?x=1',{},undefined],['/echo',{method:'POST',headers:{'X-Custom':'test'},auth:'user:pass'},'hello'],
 ['/echo',{method:'POST'},'chunks'],['/echo',{method:'PUT'},'stream'],['/chunked',{},undefined],
 ['/head',{method:'HEAD'},undefined],['/empty',{},undefined],['/eof',{},undefined],['/info',{},undefined]
])rows.push(await exchange(path,options,upload));
const getResult=await new Promise((resolve,reject)=>{
 const req=http.get(base+'/echo', {agent:false},res=>{res.setEncoding('utf8');let data='';res.on('data',chunk=>data+=chunk);res.on('end',()=>resolve(JSON.parse(data)));});req.on('error',reject);
});
const large=await new Promise((resolve,reject)=>{
 const req=http.get({hostname:'127.0.0.1',port:PORT,path:'/large',agent:false},res=>{
  res.on('error',reject);res.pause();let n=0;const hash=createHash('sha256');
  res.on('data',chunk=>{n+=chunk.length;hash.update(chunk)});
  setTimeout(()=>{if(n)reject(Error('Paused response leaked data'));res.resume()},20);
  res.on('end',()=>resolve({n,hash:hash.digest('hex'),complete:res.complete}));
 });req.on('error',reject);
});
const errors=[];
for(const path of ['/malformed','/overflow','/ambiguous'])await new Promise((resolve,reject)=>{
 const req=http.get(base+path,{agent:false,maxHeaderSize:1024},res=>{res.resume();reject(Error('Invalid response accepted'))});
 req.on('error',e=>{errors.push(e.code);resolve()});
});
const truncated=await new Promise(resolve=>{
 const trace=[];const req=http.get(base+'/truncated',{agent:false},res=>{
  res.on('aborted',()=>trace.push('aborted'));res.on('error',e=>trace.push(e.code));
  res.on('close',()=>resolve({trace,complete:res.complete,aborted:res.aborted}));res.resume();
 });req.on('error',e=>trace.push('request:'+e.code));
});
const timeout=await new Promise(resolve=>{
 const req=http.get(base+'/hang',{agent:false});req.on('timeout',()=>req.destroy(Object.assign(Error('deadline'),{code:'DEADLINE'})));
 req.on('error',e=>resolve(e.code));req.setTimeout(20);
});
const abort=await new Promise(resolve=>{
 const req=http.request(base+'/hang',{agent:false});req.on('error',()=>{});req.on('close',()=>resolve({aborted:req.aborted,destroyed:req.destroyed}));req.abort();
});
const lifecycle=await new Promise((resolve,reject)=>{
 let closed=false,ended=false;const trace=[];
 const finish=()=>{if(closed&&ended)resolve({trace,destroyed:req.destroyed})};
 const req=http.get(base+'/echo',{agent:false},res=>{
  res.on('error',reject);res.on('end',()=>{ended=true;trace.push('end');finish()});res.pause();
  setTimeout(()=>res.resume(),20);
 });req.on('error',reject);req.on('close',()=>{closed=true;trace.push('close');finish()});
});
const lookup=await new Promise(resolve=>{
 const req=http.get({hostname:'invalid.test',port:80,agent:false,lookup(_host,_options,callback){callback(Object.assign(Error('lookup'),{code:'LOOKUP_CONTROL'}))}});
 req.on('error',e=>resolve(e.code));
});
console.log(JSON.stringify({rows,getResult,large,errors,truncated,timeout,abort,lifecycle,lookup}));
})().catch(e=>{console.error(e);process.exitCode=1});
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required')
    with socketserver.ThreadingTCPServer(('127.0.0.1', 0), Peer) as server:
        server.daemon_threads = True
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        port = server.server_address[1]
        source = SOURCE.replace('BASE', json.dumps(f'http://127.0.0.1:{port}')).replace('PORT', str(port))
        try:
            with tempfile.TemporaryDirectory(prefix='mite-http-client-') as directory:
                fixture = Path(directory) / 'client.cjs'
                fixture.write_text(source)
                observations = []
                for exe in [args.node, args.mite]:
                    result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=25)
                    assert result.returncode == 0 and not result.stderr, result
                    observations.append(json.loads(result.stdout))
                assert observations[0] == observations[1], {key: [observations[0][key], observations[1][key]] for key in observations[0] if observations[0][key] != observations[1][key]}
                cases = ["({port:-1})", "({port:'bad'})", "({port:65536})", "({method:'bad method'})",
                         "({method:3})", "({path:'/a b'})", "({path:'/a\\r\\nb'})", "({headers:{'bad name':'x'}})",
                         "({headers:{x:'a\\r\\nb'}})", "({maxHeaderSize:-1})", "({maxHeaderSize:'1'})",
                         "('https://localhost/')", "({protocol:'https:'})", "(42)", "('http://localhost/',1)",
                         "('http://localhost/',{},42)"]
                validation = "const http=require('http'),rows=[];function observe(fn){try{const req=fn();req.on('error',()=>{});req.destroy();rows.push('accepted')}catch(e){rows.push([e.name,e.code])}}\n"
                validation += '\n'.join(f'observe(()=>http.request{case});' for case in cases)
                validation += '\nconsole.log(JSON.stringify(rows));'
                fixture.write_text(validation)
                errors = []
                for exe in [args.node, args.mite]:
                    result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
                    assert result.returncode == 0 and not result.stderr, result
                    errors.append(json.loads(result.stdout))
                assert errors[0] == errors[1], list(zip(cases, *errors))
                assert [i for i, row in enumerate(errors[0]) if row == 'accepted'] == [13, 14], errors
                fixture.write_text("const http=require('http');for(const options of [{agent:{}},{createConnection(){}},{socketPath:'/x'},{signal:{}},{insecureHTTPParser:true},{uniqueHeaders:[]},{IncomingMessage:class{}},{localAddress:'127.0.0.1'},{headers:{upgrade:'test'}}]){let caught=false;try{http.request(options)}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;caught=true}if(!caught)throw Error('Unsupported option accepted')}console.log('POLICY_OK');")
                result = subprocess.run([args.mite, str(fixture)], capture_output=True, text=True, timeout=10)
                assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'POLICY_OK', result
                fixture.write_text(r"""
const http=require('http');
for(const [kind,expected] of [['path','ERR_UNESCAPED_CHARACTERS'],['header','ERR_INVALID_CHAR']]){
 const req=http.request({hostname:'127.0.0.1',port:1});req.on('error',()=>{});
 if(kind==='path')req.path='/safe\r\nInjected: yes';
 else{const values=['safe'];req.setHeader('X-Test',values);values[0]='safe\r\nInjected: yes'}
 let code;try{req.flushHeaders()}catch(e){code=e.code}finally{req.destroy()}
 if(code!==expected)throw Error('Mutable request injection accepted: '+code);
}
console.log('MUTATION_REJECTED');
""")
                result = subprocess.run([args.mite, str(fixture)], capture_output=True, text=True, timeout=10)
                assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'MUTATION_REJECTED', result
                fixture.write_text("const http=require('http');http.get(" + json.dumps(f'http://127.0.0.1:{port}/upgrade') + ",req=>{throw Error('Upgrade accepted')}).on('error',e=>{if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;console.log('UPGRADE_REJECTED')});")
                result = subprocess.run([args.mite, str(fixture)], capture_output=True, text=True, timeout=10)
                assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'UPGRADE_REJECTED', result
                fixture.write_text("require('http').get(" + json.dumps(f'http://127.0.0.1:{port}/echo') + ",()=>{throw Error('http-client-fatal')});")
                for exe in [args.node, args.mite]:
                    result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
                    assert result.returncode == 1 and 'http-client-fatal' in result.stderr, result
                print('PASS native HTTP client: GET/POST/PUT, URL/options, streamed/chunked uploads, fragmented responses, headers/trailers, HEAD/204/EOF/information, 2 MiB paused response, parse/overflow/truncation errors, timeout and abort')
        finally:
            server.shutdown()
            thread.join()


if __name__ == '__main__':
    main()
