import net from 'node:net';

const callbacks=[],lengths=[];
const server=net.createServer({allowHalfOpen:true},socket=>{
 let received='';socket.setEncoding('utf8');
 socket.on('error',error=>{throw error});
 socket.on('data',text=>{received+=text});
 socket.on('end',()=>{
  if(received!=='ab')throw Error('Fast/empty writes lost bytes or order');
  socket.end('ACK');
 });
});
server.listen(0,'127.0.0.1',()=>{
 const socket=net.connect(server.address().port,'127.0.0.1',()=>{
  let inside=true;
  for(const [name,data] of [['first','a'],['empty',Buffer.alloc(0)],['last','b']]){
   socket.write(data,error=>{
    if(error)throw error;
    if(inside)throw Error('Public write callback ran synchronously');
    callbacks.push(name);
   });
   lengths.push(socket.writableLength);
  }
  inside=false;socket.end();
 });
 let reply='';socket.setEncoding('utf8');
 socket.on('data',text=>{reply+=text});
 socket.on('error',error=>{throw error});
 socket.on('close',()=>{
  if(reply!=='ACK')throw Error('Missing peer acknowledgement');
  console.log(JSON.stringify({lengths,callbacks,bytes:socket.bytesWritten}));
  server.close();
 });
});
