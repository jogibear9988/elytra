import {createServer} from 'node:http';
import {connect} from 'node:net';
import {once} from 'node:events';
const cases=[
 ['method','BOGUS / HTTP/1.1\r\nHost: test\r\n\r\n'],
 ['name','GET / HTTP/1.1\r\nHost: test\r\nInvalid Header: bad\r\n\r\n'],
 ['length','POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx'],
 ['framing','POST / HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\nContent-Length: 1\r\n\r\n0\r\n\r\n'],
 ['chunk','POST / HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n'],
 ['overflow','GET / HTTP/1.1\r\nHost: test\r\nX-Long: '+ 'x'.repeat(4096)+'\r\n\r\n'],
 ['truncated','POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 8\r\n\r\nx']
];
for(const [name,request] of cases) {
 let code;
 const server=createServer({maxHeaderSize:1024},(req,res)=>{req.on('error',()=>{});});
 server.on('clientError',(err,socket)=>{code=err.code;socket.end('HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n');});
 server.listen(0,'127.0.0.1');await once(server,'listening');
 const socket=connect(server.address().port,'127.0.0.1');socket.end(request);
 for await(const _ of socket){}
 console.log(name,code);
 await new Promise(resolve=>server.close(resolve));
}
