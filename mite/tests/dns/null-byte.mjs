import {lookup} from 'node:dns/promises';
try {console.log(JSON.stringify(await lookup('127.0.0.1\0ignored')));}
catch(e) {console.log(JSON.stringify({code:e.code,syscall:e.syscall,hostname:e.hostname}));}
