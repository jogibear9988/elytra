const net=require('node:net');
net.createServer(()=>{throw Error('native socket callback failed');}).listen(0,'127.0.0.1',function(){
 net.connect(this.address().port,'127.0.0.1');
});
