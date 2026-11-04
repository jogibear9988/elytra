(function(host) {
    'use strict';
    const native=host.native,tick=host.process.nextTick,ascii=host.domainToASCII;
    const ADDRCONFIG=native.ADDRCONFIG,V4MAPPED=native.V4MAPPED,ALL=native.ALL;
    let defaultOrder='verbatim';
    function error(Type,code,message) {const e=new Type(message);e.code=code;return e;}
    function order(value) {
        if(!['verbatim','ipv4first','ipv6first'].includes(value))throw error(TypeError,'ERR_INVALID_ARG_VALUE','Invalid DNS result order');
        return value;
    }
    function settings(value) {
        if(value==null)value={};
        if(typeof value==='number')value={family:value};
        if(typeof value!=='object')throw error(TypeError,'ERR_INVALID_ARG_TYPE','options must be a number or object');
        let family=value.family??0;
        if(family==='IPv4')family=4;if(family==='IPv6')family=6;
        if(![0,4,6].includes(family))throw error(TypeError,'ERR_INVALID_ARG_VALUE','Invalid address family');
        let hints=value.hints??0;
        if(typeof hints!=='number')throw error(TypeError,'ERR_INVALID_ARG_TYPE','hints must be a number');
        hints=hints|0;
        if(hints&~(ADDRCONFIG|V4MAPPED|ALL))
            throw error(TypeError,'ERR_INVALID_ARG_VALUE','Invalid getaddrinfo hints');
        for(const key of ['all','verbatim'])if(value[key]!=null && typeof value[key]!=='boolean')
            throw error(TypeError,'ERR_INVALID_ARG_TYPE',`${key} must be a boolean`);
        let resultOrder=defaultOrder;
        if(value.verbatim!=null)resultOrder=value.verbatim?'verbatim':'ipv4first';
        if(value.order!=null)resultOrder=order(value.order);
        return {family,hints,all:value.all??false,order:resultOrder};
    }
    function failure(status,hostname) {
        const {code}=native.error(status);
        const e=error(Error,code,`getaddrinfo ${code} ${hostname}`);
        e.errno=status;e.syscall='getaddrinfo';e.hostname=hostname;return e;
    }
    function lookup(hostname,opts,callback) {
        if(typeof opts==='function'){callback=opts;opts=undefined;}
        if(typeof callback!=='function')throw error(TypeError,'ERR_INVALID_ARG_TYPE','callback must be a function');
        if(hostname && typeof hostname!=='string')throw error(TypeError,'ERR_INVALID_ARG_TYPE','hostname must be a string');
        const config=settings(opts);
        if(!hostname){tick(()=>config.all?callback(null,[]):callback(null,null,config.family===6?6:4));return {};}
        const zone=hostname.indexOf('%');
        const family=zone!==-1&&!/^[0-9A-Za-z.:-]+$/.test(hostname.slice(zone+1))?0:native.isIP(hostname);
        if(family){tick(()=>config.all?callback(null,[{address:hostname,family}]):callback(null,hostname,family));return {};}
        // Pass ASCII to OS resolution while preserving the caller's original
        // hostname in any error. Embedded NUL must never resolve a prefix.
        let normalized=hostname.includes('\0')?'':ascii(hostname);
        const complete=(status,values)=>{
            if(status){callback(failure(status,hostname));return;}
            if(config.order!=='verbatim') {
                const first=config.order==='ipv4first'?4:6;
                values=values.filter(value=>value.family===first).concat(values.filter(value=>value.family!==first));
            }
            if(config.all)callback(null,values);else callback(null,values[0].address,values[0].family);
        };
        const status=native.lookup(normalized||'\0',config.family,config.hints,complete);
        if(status)tick(()=>complete(status,[]));
        return {};
    }
    function setDefaultResultOrder(value){defaultOrder=order(value);}
    function getDefaultResultOrder(){return defaultOrder;}
    const promises={lookup:async(hostname,opts)=>{
        // Node's Promise API rejects family strings although its callback API
        // accepts the legacy IPv4/IPv6 aliases.
        if(opts && typeof opts==='object' && typeof opts.family==='string')
            throw error(TypeError,'ERR_INVALID_ARG_VALUE','Invalid address family');
        return new Promise((resolve,reject)=>lookup(hostname,opts,(e,address,family)=>{
            if(e)reject(e);else resolve(Array.isArray(address)?address:{address,family});
        }));
    },setDefaultResultOrder,getDefaultResultOrder};
    return {lookup,promises,setDefaultResultOrder,getDefaultResultOrder,ADDRCONFIG,V4MAPPED,ALL};
})
