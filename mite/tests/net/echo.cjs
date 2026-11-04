const net=require('node:net');
const {Duplex}=require('node:stream');
const {EventEmitter}=require('node:events');
let accepted,connected;
const server=net.createServer(socket=>{
 accepted=[socket instanceof net.Socket,socket instanceof Duplex,socket.remoteFamily,socket.remoteAddress,socket.address().family,socket.localAddress===socket.address().address,socket.remotePort>0];
 socket.setNoDelay().setKeepAlive(true,1000).setEncoding('utf8');
 socket.on('data',value=>socket.write(value.toUpperCase()));
 socket.on('error',e=>console.log('server socket error',e.code));
});
console.log(server instanceof EventEmitter,server.listening,server.address());
server.listen({port:0,host:'127.0.0.1'},()=>{
 console.log('listening',server.listening,server.address().family);
 const client=net.connect(server.address().port,'127.0.0.1',()=>{
  connected=[client.connecting,client.pending,client.readyState,client.remoteFamily,client.remoteAddress];
 });
 client.setEncoding('utf8');client.write('hëllo');
 let text='';client.on('data',value=>{text+=value;if(text==='HËLLO')client.end();});
 client.on('error',e=>console.log('client error',e.code));
 client.on('close',had=>{
  console.log('accepted',...accepted);console.log('connect',...connected);
  console.log('result',text,had,client.bytesRead,client.bytesWritten,client.destroyed);
  server.getConnections((err,count)=>{console.log('connections',err,count);server.close(()=>console.log('closed',server.listening,server.address()));});
 });
});
