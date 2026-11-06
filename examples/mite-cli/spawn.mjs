import {spawn} from 'node:child_process';
import {once} from 'node:events';

// Run this example with the standalone CLI. Its child uses the same runtime.
const child=spawn(process.execPath,['-e',"console.log('child pid',process.pid,'parent',process.ppid)"],{
    stdio:['ignore','pipe','inherit']
});
const closed=once(child,'close');
child.stdout.setEncoding('utf8');
for await(const text of child.stdout)console.log(text.trimEnd());
const [code,signal]=await closed;
console.log('child finished',code,signal);
if(code!==0)process.exitCode=1;
