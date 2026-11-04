import {readFile} from 'node:fs/promises';
import {lookup} from 'node:dns/promises';
const workers=process.argv.slice(2).map(path=>readFile(path,'utf8'));
let resolved=false;
const result=lookup('localhost',{family:4}).then(value=>{resolved=true;return value;});
let timer=false;
const timerDone=new Promise(resolve=>setTimeout(()=>{timer=true;console.log('timer',resolved);resolve();},15));
const value=await result;await timerDone;
console.log('lookup',timer,value.address,value.family,(await Promise.all(workers)).every(value=>value==='x'));
