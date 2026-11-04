import {Readable} from 'node:stream';
const values=[];for await(const value of Readable.from([1,2,3]))values.push(value);console.log(values.join(','));
let cleaned=false;async function* generator(){try{yield 1;yield 2;}finally{cleaned=true;}}
const early=Readable.from(generator());for await(const value of early){console.log(value);break;}
console.log(early.destroyed,cleaned);
const keep=Readable.from([1,2,3]);for await(const value of keep.iterator({destroyOnReturn:false})){console.log(value);break;}
const rest=[];for await(const value of keep)rest.push(value);console.log(rest.join(','),keep.readableEnded);
