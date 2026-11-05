(function (host) {
    'use strict';
    const native=host.native;
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
    const os={arch,availableParallelism,endianness,freemem,homedir,hostname,loadavg,platform,release,tmpdir,totalmem,type,uptime,version,machine};
    for(const name of ['arch','availableParallelism','endianness','freemem','homedir','hostname','platform','release','tmpdir','totalmem','type','uptime','version','machine']){
        const fn=os[name];fn[Symbol.toPrimitive]=()=>fn();
    }
    for(const name of ['cpus','networkInterfaces','userInfo','getPriority','setPriority']){
        os[name]=function(){const error=new Error(`os.${name} is not implemented`);error.code='ERR_MITE_UNSUPPORTED_API';throw error;};
    }
    Object.defineProperties(os,{
        constants:{get(){const error=new Error('os.constants is not implemented');error.code='ERR_MITE_UNSUPPORTED_API';throw error;},enumerable:true},
        EOL:{value:windows?'\r\n':'\n',enumerable:true,configurable:true},
        devNull:{value:windows?'\\\\.\\nul':'/dev/null',enumerable:true,configurable:true}
    });
    return os;
})
