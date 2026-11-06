import {execFile} from 'node:child_process';

execFile(process.execPath, ['-e', "console.log('buffered native execution')"],
  {timeout: 2000, maxBuffer: 4096}, (error, stdout) => {
    if (error) { console.error(error.message); process.exitCode = 1; }
    else console.log(stdout.trimEnd());
  });
