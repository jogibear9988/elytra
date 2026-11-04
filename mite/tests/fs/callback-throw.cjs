const fs=require('node:fs');
fs.readFile('fixture.txt',()=>{
    console.log('callback');
    fs.readFile('fixture.txt',()=>console.log('unexpected late callback'));
    throw new Error('fs callback fixture');
});
