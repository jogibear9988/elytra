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
    function randomInt(min,max,callback){
        if(max===undefined || typeof max==='function'){callback=max;max=min;min=0;}
        if(callback!==undefined && typeof callback!=='function')fail('callback',TypeError,'ERR_INVALID_ARG_TYPE');
        if(!Number.isSafeInteger(min))fail('min',TypeError,'ERR_INVALID_ARG_TYPE');
        if(!Number.isSafeInteger(max))fail('max',TypeError,'ERR_INVALID_ARG_TYPE');
        const range=max-min,space=281474976710656; // Exactly 2**48.
        if(max<=min || range>=space)fail('max - min',RangeError,'ERR_OUT_OF_RANGE');
        const limit=space-space%range;
        function select(bytes){
            let value=0;
            for(let i=0;i<6;i++)value=value*256+bytes[i];
            return value<limit?min+value%range:undefined;
        }
        if(callback===undefined){
            for(;;){const result=select(native.random(6));if(result!==undefined)return result;}
        }
        function attempt(){
            native.random(6,(error,bytes)=>{
                if(error){callback(error);return;}
                const result=select(bytes);
                if(result===undefined){attempt();return;}
                callback(undefined,result);
            });
        }
        // Fresh native worker entropy per attempt, without an integer cache.
        attempt();
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
    const hmacs=new WeakMap();
    function hmacState(value){const state=hmacs.get(value);if(!state)throw new TypeError('Invalid Hmac receiver');return state;}
    function binaryBytes(value){
        if(value===undefined)fail('key or buffer',TypeError,'ERR_INVALID_ARG_TYPE');
        return host.encoding.byteView(value);
    }
    function keyEncoding(options){
        // Like Node, inspect encoding even for binary keys, before validating key.
        const label=options==null?undefined:options.encoding;
        if(label!=null && typeof label!=='string')fail('options.encoding',TypeError,'ERR_INVALID_ARG_TYPE');
        return label;
    }
    function Hmac(algorithm,key,options){
        if(!new.target)return new Hmac(algorithm,key,options);
        if(typeof algorithm!=='string')fail('algorithm',TypeError,'ERR_INVALID_ARG_TYPE');
        const label=keyEncoding(options);
        let bytes;
        if(typeof key==='string'){
            if(label && label!=='buffer' && !Buffer.isEncoding(label))fail('encoding',TypeError,'ERR_UNKNOWN_ENCODING');
            bytes=Buffer.from(key,!label || label==='buffer'?'utf8':label);
        }else bytes=binaryBytes(key);
        const handle=native.hmac.create(algorithm,bytes);
        hmacs.set(this,{handle,finalized:false,options});
        // Node initializes crypto stream state only when stream methods need it.
        // Manual digest must not eagerly validate a readable output encoding.
    }
    Object.setPrototypeOf(Hmac.prototype,Transform.prototype);
    Object.setPrototypeOf(Hmac,Transform);
    for(const name of ['_readableState','_writableState'])Object.defineProperty(Hmac.prototype,name,{
        configurable:true,
        get(){Transform.call(this,hmacState(this).options);return this[name];},
        set(value){Object.defineProperty(this,name,{value,writable:true,enumerable:true,configurable:true});}
    });
    Hmac.prototype.update=function update(data,inputEncoding){
        const state=hmacState(this);if(state.finalized)finalized();
        if(!state.handle.update(inputBytes(data,inputEncoding)))fail('HMAC update',Error,'ERR_CRYPTO_HASH_UPDATE_FAILED');
        return this;
    };
    Hmac.prototype.digest=function digest(outputEncoding){
        const state=hmacState(this);
        // Node returns empty output on repeated HMAC digest (unlike Hash).
        if(state.finalized)return outputEncoding && outputEncoding!=='buffer'?'':Buffer.alloc(0);
        const label=outputEncoding?`${outputEncoding}`:undefined;
        const value=buffer(state.handle.digest());state.finalized=true;
        const codec=encoding(label);return codec?value.toString(codec):value;
    };
    Hmac.prototype._transform=function(chunk,codec,callback){
        try{this.update(chunk,codec);}catch(error){callback(error);return;}
        callback();
    };
    Hmac.prototype._flush=function(callback){
        let value;
        try{value=this.digest();}catch(error){callback(error);return;}
        this.push(value);callback();
    };
    function createHmac(algorithm,key,options){return new Hmac(algorithm,key,options);}
    function timingSafeEqual(left,right){return native.hmac.equal(binaryBytes(left),binaryBytes(right));}
    return {randomBytes,randomUUID,randomInt,createHash,Hash,createHmac,Hmac,timingSafeEqual};
})
