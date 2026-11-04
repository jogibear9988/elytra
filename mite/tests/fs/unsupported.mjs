import fs from 'node:fs';
for(const operation of [()=>fs.readFileSync('unused',{signal:{}}),()=>fs.writeFileSync('unused','x',{flush:true}),
                       ()=>fs.readdirSync('.',{recursive:true}),()=>fs.rmdirSync('unused',{recursive:true})]) {
    try{operation();}catch(error){console.log(error.code);}
}
