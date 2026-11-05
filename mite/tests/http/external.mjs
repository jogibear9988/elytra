import {createServer} from 'node:http';
let connections=0,requests=0;
const server=createServer(async(req,res)=>{
 requests++;
 if(req.url==='/echo'){
  const chunks=[];for await(const chunk of req)chunks.push(chunk);
  res.writeHead(202,{'Content-Type':'application/octet-stream'});res.end(Buffer.concat(chunks));
 }else if(req.url==='/stream'){
  res.setHeader('Content-Type','text/plain');res.write('first');setTimeout(()=>res.end('second'),2);
 }else if(req.url==='/empty'){res.statusCode=204;res.end();}
 else if(req.url==='/done'){res.end('done');res.once('finish',()=>server.close(()=>console.log('closed',connections,requests)));}
 else{res.setHeader('Content-Type','application/json');res.end(JSON.stringify({method:req.method,path:req.url,agent:req.headers['user-agent']}));}
});
server.on('connection',()=>connections++);
server.listen(0,'127.0.0.1',()=>console.log(server.address().port));
