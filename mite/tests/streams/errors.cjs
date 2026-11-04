const {Readable,Transform,Writable,pipeline}=require('node:stream');
const failure=new Error('transform fixture');const source=Readable.from([1,2]);
const transform=new Transform({objectMode:true,transform(chunk,encoding,callback){callback(failure);}});
const destination=new Writable({objectMode:true,write(chunk,encoding,callback){callback();}});
pipeline(source,transform,destination,error=>console.log(error===failure,source.destroyed,transform.destroyed,destination.destroyed));
const invalid=Readable.from([null]);invalid.on('error',error=>console.log(error.code));invalid.resume();
const target=new Writable({write(chunk,encoding,callback){callback();}});target.on('error',error=>console.log(error.code));
target.end();console.log(target.write('late',error=>console.log('callback',error.code)));
