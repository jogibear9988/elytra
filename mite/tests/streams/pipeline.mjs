import {Readable,Transform,Writable} from 'node:stream';
import {pipeline,finished} from 'node:stream/promises';
async function* input(){yield 1;await Promise.resolve();yield 2;yield 3;}
const source=Readable.from(input());const values=[];
const transform=new Transform({objectMode:true,transform(value,encoding,callback){queueMicrotask(()=>callback(null,value*2));}});
const destination=new Writable({objectMode:true,write(value,encoding,callback){values.push(value);callback();}});
await pipeline(source,transform,destination);
console.log(values.join(','),source.destroyed,transform.destroyed,destination.destroyed);
await finished(destination);console.log('finished');
const bytes=[];await pipeline(Readable.from(['é','文']),new Transform({transform(chunk,encoding,callback){callback(null,chunk);}}),
    new Writable({write(chunk,encoding,callback){bytes.push(chunk.toString());callback();}}));
console.log(bytes.join(','));
