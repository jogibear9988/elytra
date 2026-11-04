import {Readable,Transform,Writable} from 'node:stream';
import {pipeline} from 'node:stream/promises';
const failure=new Error('transform fixture'),source=Readable.from([1,2]);
const transform=new Transform({objectMode:true,transform(chunk,encoding,callback){callback(failure);}});
const destination=new Writable({objectMode:true,write(chunk,encoding,callback){callback();}});
try{await pipeline(source,transform,destination);}catch(error){console.log(error===failure,source.destroyed,transform.destroyed,destination.destroyed);}
await new Promise(resolve=>{const invalid=Readable.from([null]);invalid.on('error',error=>{console.log(error.code);resolve();});invalid.resume();});
await new Promise(resolve=>{
    const target=new Writable({write(chunk,encoding,callback){callback();}});target.on('error',error=>{console.log(error.code);resolve();});
    target.end();console.log(target.write('late',error=>console.log('callback',error.code)));
});
