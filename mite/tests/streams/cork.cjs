const {Writable}=require('node:stream');const events=[];
const target=new Writable({write(chunk,encoding,callback){events.push('single:'+chunk);callback();},
    writev(chunks,callback){events.push(chunks.map(c=>c.chunk.toString()).join('|'));callback();}});
target.on('close',()=>console.log(events.join(',')));
target.cork();target.write('a',()=>events.push('a'));target.cork();target.write('b',()=>events.push('b'));
console.log(target.writableCorked);target.uncork();console.log(target.writableCorked);target.uncork();target.end(()=>events.push('end'));
