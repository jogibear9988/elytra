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
    const hashes=new WeakMap();
    const Transform=host.stream.Transform;
    function stateFor(hash){const state=hashes.get(hash);if(!state)throw new TypeError('Invalid Hash receiver');return state;}
    function finalized(){fail('hash finalized',Error,'ERR_CRYPTO_HASH_FINALIZED');}
    function outputLength(options){
        const length=options!==null && typeof options==='object'?options.outputLength:undefined;
        if(length===undefined)return undefined;
        if(typeof length!=='number')fail('options.outputLength',TypeError,'ERR_INVALID_ARG_TYPE');
        if(!Number.isInteger(length) || length<0 || length>0xffffffff)fail('options.outputLength',RangeError,'ERR_OUT_OF_RANGE');
        if(length>0x7fffffff)fail('output length above Mite Buffer limit',RangeError,'ERR_MITE_UNSUPPORTED_API');
        return length;
    }
    function Hash(algorithm,options){
        if(!new.target)return new Hash(algorithm,options);
        if(typeof algorithm!=='string')fail('algorithm',TypeError,'ERR_INVALID_ARG_TYPE');
        const handle=native.hash.create(algorithm,outputLength(options));
        hashes.set(this,{handle,finalized:false});
        Transform.call(this,options);
    }
    Object.setPrototypeOf(Hash.prototype,Transform.prototype);
    Object.setPrototypeOf(Hash,Transform);
    function encoding(value){
        if(typeof value!=='string')return undefined;
        const zero=value.indexOf('\0');if(zero!==-1)value=value.slice(0,zero);
        value=value.toLowerCase();
        return Buffer.isEncoding(value)?value:undefined;
    }
    function inputBytes(data,inputEncoding){
        if(typeof data==='string'){
            if(typeof inputEncoding==='string' && inputEncoding.toLowerCase()==='hex' && data.length%2)fail('encoding',TypeError,'ERR_INVALID_ARG_VALUE');
            return Buffer.from(data,encoding(inputEncoding)||'utf8');
        }
        if(!ArrayBuffer.isView(data))fail('data',TypeError,'ERR_INVALID_ARG_TYPE');
        return host.encoding.byteView(data);
    }
    Hash.prototype.update=function update(data,inputEncoding){
        const state=stateFor(this);if(state.finalized)finalized();
        if(!state.handle.update(inputBytes(data,inputEncoding)))fail('hash update',Error,'ERR_CRYPTO_HASH_UPDATE_FAILED');
        return this;
    };
    Hash.prototype.digest=function digest(outputEncoding){
        const state=stateFor(this);if(state.finalized)finalized();
        const label=outputEncoding?`${outputEncoding}`:undefined;
        const value=buffer(state.handle.digest());state.finalized=true;
        const codec=encoding(label);return codec?value.toString(codec):value;
    };
    Hash.prototype.copy=function copy(options){
        const state=stateFor(this);if(state.finalized)finalized();
        const handle=state.handle.copy(outputLength(options));
        const result=Object.create(Hash.prototype);hashes.set(result,{handle,finalized:false});
        Transform.call(result,options);return result;
    };
    Hash.prototype._transform=function(chunk,codec,callback){
        try{stateFor(this).handle.update(inputBytes(chunk,codec));}catch(error){callback(error);return;}
        callback();
    };
    Hash.prototype._flush=function(callback){
        let value;
        try{value=buffer(stateFor(this).handle.digest());}catch(error){callback(error);return;}
        this.push(value);callback();
    };
    function createHash(algorithm,options){return new Hash(algorithm,options);}
    return {randomBytes,randomUUID,createHash,Hash};
})
