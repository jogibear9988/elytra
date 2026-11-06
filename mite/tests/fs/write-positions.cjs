const fs=require('node:fs');
(async()=>{
    const rows=[];
    for(const position of [-1,-2,-3,-Number.MAX_SAFE_INTEGER,0,2,null,undefined,1.5,-Infinity,-1n]){
        for(const kind of ['sync','callback','filehandle']){
            const filename='write-position.bin';
            const handle=kind==='filehandle'?await fs.promises.open(filename,'w+'):null;
            const fd=handle?handle.fd:fs.openSync(filename,'w+');
            fs.writeSync(fd,'abcdef');
            let outcome;
            try{
                if(kind==='sync')outcome=fs.writeSync(fd,Buffer.from('X'),0,1,position);
                else if(kind==='callback')outcome=await new Promise((resolve,reject)=>fs.write(fd,Buffer.from('X'),0,1,position,(error,count)=>error?reject(error):resolve(count)));
                else outcome=(await handle.write(Buffer.from('X'),0,1,position)).bytesWritten;
            }catch(error){outcome=[error.name,error.code];}
            const after=fs.readFileSync(filename).toString('hex');
            // An ordinary write exposes whether the preceding offset advanced
            // the descriptor cursor; a separate readFile descriptor cannot do so.
            fs.writeSync(fd,'Y');
            const final=fs.readFileSync(filename).toString('hex');
            rows.push([String(position),kind,outcome,after,final]);
            if(handle)await handle.close();else fs.closeSync(fd);
        }
    }
    console.log(JSON.stringify(rows));
})().catch(error=>{console.error(error);process.exitCode=1;});
