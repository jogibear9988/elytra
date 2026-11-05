(function (host) {
    'use strict';
    const native=host.native;
    const Buffer=host.Buffer;
    const windows=host.process.platform==='win32';
    const {type:osType,release:osRelease,version:osVersion,machine:osMachine}=native.information;
    const byteOrder=native.endianness;
    function arch(){return process.arch;}
    function platform(){return process.platform;}
    function endianness(){return byteOrder;}
    function type(){return osType;}
    function release(){return osRelease;}
    function version(){return osVersion;}
    function machine(){return osMachine;}
    function hostname(){return native.query('hostname');}
    function homedir(){return native.query('homedir');}
    function freemem(){return native.query('freemem');}
    function totalmem(){return native.query('totalmem');}
    function uptime(){return native.query('uptime');}
    function availableParallelism(){return native.query('availableParallelism');}
    function loadavg(){return native.query('loadavg');}
    function priorityArgument(value,name,min=-2147483648,max=2147483647){
        let error;
        if(typeof value!=='number'){
            error=new TypeError(`The "${name}" argument must be a number`);error.code='ERR_INVALID_ARG_TYPE';
        }else if(!Number.isInteger(value) || value<min || value>max){
            error=new RangeError(`The "${name}" argument must be an integer between ${min} and ${max}`);error.code='ERR_OUT_OF_RANGE';
        }
        if(error)throw error;
    }
    function getPriority(pid){
        if(pid===undefined)pid=0;
        else priorityArgument(pid,'pid');
        return native.getPriority(pid);
    }
    function setPriority(pid,priority){
        if(priority===undefined){priority=pid;pid=0;}
        priorityArgument(pid,'pid');
        priorityArgument(priority,'priority',-20,19);
        native.setPriority(pid,priority);
    }
    function networkInterfaces(){return native.query('networkInterfaces');}
    function cpus(){return native.query('cpus');}
    function userInfo(options){
        let encoding=options!==null && typeof options==='object'?options.encoding:undefined;
        if(typeof encoding!=='string')encoding='utf8';
        else {
            const zero=encoding.indexOf('\0');
            if(zero!==-1)encoding=encoding.slice(0,zero);
            encoding=encoding.toLowerCase();
        }
        const asBuffer=encoding==='buffer';
        if(!asBuffer && !Buffer.isEncoding(encoding))encoding='utf8';
        const data=native.query('userInfo');
        const result=Object.create(null);
        result.uid=data.uid;result.gid=data.gid;
        for(const name of ['username','homedir','shell']){
            if(data[name]===null){result[name]=null;continue;}
            const bytes=Buffer.from(data[name]);
            result[name]=asBuffer?bytes:bytes.toString(encoding);
        }
        return result;
    }
    function tmpdir(){
        let directory;
        if(windows){
            directory=native.getenv('TEMP') || native.getenv('TMP') || `${native.getenv('SystemRoot') || native.getenv('windir')}\\temp`;
            if(directory.length>1 && directory.endsWith('\\') && directory.at(-2)!==':')directory=directory.slice(0,-1);
        }else{
            directory=native.getenv('TMPDIR') || native.getenv('TMP') || native.getenv('TEMP') || '/tmp';
            if(directory.length>1 && directory.endsWith('/'))directory=directory.slice(0,-1);
        }
        return directory;
    }
    const os={arch,availableParallelism,cpus,userInfo,networkInterfaces,getPriority,setPriority,endianness,freemem,homedir,hostname,loadavg,platform,release,tmpdir,totalmem,type,uptime,version,machine};
    for(const name of ['arch','availableParallelism','endianness','freemem','homedir','hostname','platform','release','tmpdir','totalmem','type','uptime','version','machine']){
        const fn=os[name];fn[Symbol.toPrimitive]=()=>fn();
    }
    Object.defineProperties(os,{
        constants:{get(){const error=new Error('os.constants is not implemented');error.code='ERR_MITE_UNSUPPORTED_API';throw error;},enumerable:true},
        EOL:{value:windows?'\r\n':'\n',enumerable:true,configurable:true},
        devNull:{value:windows?'\\\\.\\nul':'/dev/null',enumerable:true,configurable:true}
    });
    return os;
})
