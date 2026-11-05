(function(host){
    'use strict';
    const native=host.native,Buffer=host.Buffer;
    function fail(name,Type,code){const error=new Type(`Invalid ${name} argument`);error.code=code;throw error;}
    function buffer(bytes){return Buffer.from(bytes.buffer,bytes.byteOffset,bytes.byteLength);}
    function randomBytes(size,callback){
        if(typeof size!=='number')fail('size',TypeError,'ERR_INVALID_ARG_TYPE');
        if(Number.isNaN(size) || size<0 || size>2147483647)fail('size',RangeError,'ERR_OUT_OF_RANGE');
        size=size>>>0;
        if(callback!==undefined && typeof callback!=='function')fail('callback',TypeError,'ERR_INVALID_ARG_TYPE');
        if(callback===undefined)return buffer(native.random(size));
        if(size===0){callback(null,buffer(native.random(0)));return;}
        native.random(size,(error,bytes)=>{if(error)callback(error);else callback(null,buffer(bytes));});
    }
    function randomUUID(options){
        if(options!==undefined && (options===null || typeof options!=='object' || Array.isArray(options)))fail('options',TypeError,'ERR_INVALID_ARG_TYPE');
        const disableEntropyCache=options?.disableEntropyCache;
        if(disableEntropyCache!==undefined && typeof disableEntropyCache!=='boolean')fail('options.disableEntropyCache',TypeError,'ERR_INVALID_ARG_TYPE');
        // This implementation asks the OS for fresh entropy on every call.
        return native.uuid();
    }
    return {randomBytes,randomUUID};
})
