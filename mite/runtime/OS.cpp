#include "OS.h"
#include "OSNetmask.h"
#include <AK/HashMap.h>
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <bit>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <limits.h>
#include <vector>
#include <uv.h>

namespace Mite {
namespace {
JS::Value text(JS::VM& vm, char const* bytes)
{
    return JS::PrimitiveString::create(vm, Utf16String::from_utf8_with_replacement_character(StringView(bytes, strlen(bytes))));
}
JS::Completion system_error(JS::VM& vm, char const* syscall, int status)
{
    GC::Root info { JS::Object::create(vm.realm(), vm.realm().intrinsics().object_prototype()) };
    info->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);
    info->define_direct_property("code"_utf16, text(vm, uv_err_name(status)), JS::default_attributes);
    info->define_direct_property("message"_utf16, text(vm, uv_strerror(status)), JS::default_attributes);
    info->define_direct_property("syscall"_utf16, text(vm, syscall), JS::default_attributes);
    GC::Root error { JS::Error::create(vm.realm(), Utf16String::formatted("A system error occurred: {} returned {} ({})", syscall, uv_err_name(status), uv_strerror(status))) };
    error->define_direct_property("name"_utf16, text(vm, "SystemError"), JS::default_attributes);
    error->define_direct_property("code"_utf16, text(vm, "ERR_SYSTEM_ERROR"), JS::default_attributes);
    error->define_direct_property("info"_utf16, info.ptr(), JS::default_attributes);
    error->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);
    error->define_direct_property("syscall"_utf16, text(vm, syscall), JS::default_attributes);
    return JS::throw_completion(error.ptr());
}
JS::ThrowCompletionOr<JS::Value> cpu_info(JS::VM& vm)
{
    uv_cpu_info_t* entries=nullptr;
    int count=0;
    // Node reports unavailable CPU information as an empty list.
    if(uv_cpu_info(&entries,&count))return JS::Value(TRY(JS::Array::create(vm.realm(),0)));
    ScopeGuard release([&]{uv_free_cpu_info(entries,count);});
    GC::Root result {TRY(JS::Array::create(vm.realm(),static_cast<u32>(count)))};
    for(int i=0;i<count;++i){
        auto const& entry=entries[i];
        GC::Root record {JS::Object::create(vm.realm(),vm.realm().intrinsics().object_prototype())};
        auto bytes=ReadonlyBytes{reinterpret_cast<u8 const*>(entry.model),strlen(entry.model)};
        record->define_direct_property("model"_utf16,JS::PrimitiveString::create(vm,Utf16String::from_utf16(Utf16View::from_latin1(bytes,false))),JS::default_attributes);
        record->define_direct_property("speed"_utf16,JS::Value(entry.speed),JS::default_attributes);
        GC::Root times {JS::Object::create(vm.realm(),vm.realm().intrinsics().object_prototype())};
        times->define_direct_property("user"_utf16,JS::Value(static_cast<double>(entry.cpu_times.user)),JS::default_attributes);
        times->define_direct_property("nice"_utf16,JS::Value(static_cast<double>(entry.cpu_times.nice)),JS::default_attributes);
        times->define_direct_property("sys"_utf16,JS::Value(static_cast<double>(entry.cpu_times.sys)),JS::default_attributes);
        times->define_direct_property("idle"_utf16,JS::Value(static_cast<double>(entry.cpu_times.idle)),JS::default_attributes);
        times->define_direct_property("irq"_utf16,JS::Value(static_cast<double>(entry.cpu_times.irq)),JS::default_attributes);
        record->define_direct_property("times"_utf16,times.ptr(),JS::default_attributes);
        TRY(result->create_data_property_or_throw(static_cast<u32>(i),record.ptr()));
    }
    return result.ptr();
}
JS::ThrowCompletionOr<JS::Value> network_interfaces(JS::VM& vm)
{
    uv_interface_address_t* entries=nullptr;
    int count=0;
    int status=uv_interface_addresses(&entries,&count);
    GC::Root result {JS::Object::create(vm.realm(),vm.realm().intrinsics().object_prototype())};
    if(status==UV_ENOSYS)return result.ptr();
    if(status)return system_error(vm,"uv_interface_addresses",status);
    ScopeGuard release([&]{uv_free_interface_addresses(entries,count);});
    HashMap<Utf16String,GC::Root<JS::Array>> groups;
    for(int i=0;i<count;++i){
        auto const& entry=entries[i];
        char address[INET6_ADDRSTRLEN] {};
        char netmask[INET6_ADDRSTRLEN] {};
        bool ipv6=entry.address.address4.sin_family==AF_INET6;
        std::span<unsigned char const> mask;
        if(ipv6){
            status=uv_ip6_name(&entry.address.address6,address,sizeof(address));
            if(!status)status=uv_ip6_name(&entry.netmask.netmask6,netmask,sizeof(netmask));
            mask={reinterpret_cast<unsigned char const*>(&entry.netmask.netmask6.sin6_addr),16};
        }else if(entry.address.address4.sin_family==AF_INET){
            status=uv_ip4_name(&entry.address.address4,address,sizeof(address));
            if(!status)status=uv_ip4_name(&entry.netmask.netmask4,netmask,sizeof(netmask));
            mask={reinterpret_cast<unsigned char const*>(&entry.netmask.netmask4.sin_addr),4};
        }else return system_error(vm,"uv_interface_addresses",UV_EAFNOSUPPORT);
        if(status)return system_error(vm,ipv6?"uv_ip6_name":"uv_ip4_name",status);
        char mac[18];
        std::snprintf(mac,sizeof(mac),"%02x:%02x:%02x:%02x:%02x:%02x",
            static_cast<unsigned>(static_cast<u8>(entry.phys_addr[0])),static_cast<unsigned>(static_cast<u8>(entry.phys_addr[1])),
            static_cast<unsigned>(static_cast<u8>(entry.phys_addr[2])),static_cast<unsigned>(static_cast<u8>(entry.phys_addr[3])),
            static_cast<unsigned>(static_cast<u8>(entry.phys_addr[4])),static_cast<unsigned>(static_cast<u8>(entry.phys_addr[5])));
        GC::Root record {JS::Object::create(vm.realm(),vm.realm().intrinsics().object_prototype())};
        record->define_direct_property("address"_utf16,text(vm,address),JS::default_attributes);
        record->define_direct_property("netmask"_utf16,text(vm,netmask),JS::default_attributes);
        record->define_direct_property("family"_utf16,text(vm,ipv6?"IPv6":"IPv4"),JS::default_attributes);
        record->define_direct_property("mac"_utf16,text(vm,mac),JS::default_attributes);
        record->define_direct_property("internal"_utf16,JS::Value(entry.is_internal!=0),JS::default_attributes);
        auto prefix=os_netmask_prefix(mask);
        auto cidr=prefix?JS::Value(JS::PrimitiveString::create(vm,Utf16String::formatted("{}/{}",address,*prefix))):JS::js_null();
        record->define_direct_property("cidr"_utf16,cidr,JS::default_attributes);
        if(ipv6)record->define_direct_property("scopeid"_utf16,JS::Value(entry.address.address6.sin6_scope_id),JS::default_attributes);
        auto name=Utf16String::from_utf8_with_replacement_character(StringView(entry.name,strlen(entry.name)),Utf16String::WithBOMHandling::No);
        GC::Root<JS::Array> addresses;
        if(auto found=groups.find(name);found!=groups.end())addresses=found->value;
        else{
            addresses=TRY(JS::Array::create(vm.realm(),0));
            groups.set(name,addresses);
            // Define an own data property even for a prototype-looking OS name.
            result->define_direct_property(name,addresses.ptr(),JS::default_attributes);
        }
        auto length=static_cast<u32>(TRY(addresses->get("length"_utf16)).as_double());
        TRY(addresses->create_data_property_or_throw(length,record.ptr()));
    }
    return result.ptr();
}
JS::ThrowCompletionOr<JS::Value> user_info(JS::VM& vm)
{
    uv_passwd_t entry {};
    if(auto status=uv_os_get_passwd(&entry);status)return system_error(vm,"uv_os_get_passwd",status);
    ScopeGuard release([&]{uv_os_free_passwd(&entry);});
    GC::Root result {JS::Object::create(vm.realm(),nullptr)};
#ifdef _WIN32
    auto uid=static_cast<double>(static_cast<i32>(entry.uid & 0xffffffffu));
    auto gid=static_cast<double>(static_cast<i32>(entry.gid & 0xffffffffu));
#else
    auto uid=static_cast<double>(entry.uid);
    auto gid=static_cast<double>(entry.gid);
#endif
    result->define_direct_property("uid"_utf16,JS::Value(uid),JS::default_attributes);
    result->define_direct_property("gid"_utf16,JS::Value(gid),JS::default_attributes);
    auto copy=[&](char const* bytes)->JS::ThrowCompletionOr<JS::Value>{
        if(!bytes)return JS::js_null();
        auto size=strlen(bytes);
        auto array=TRY(JS::Uint8Array::create(vm.realm(),size));
        if(size)array->viewed_array_buffer()->overwrite(0,bytes,size);
        return JS::Value(array);
    };
    result->define_direct_property("username"_utf16,TRY(copy(entry.username)),JS::default_attributes);
    result->define_direct_property("homedir"_utf16,TRY(copy(entry.homedir)),JS::default_attributes);
    result->define_direct_property("shell"_utf16,TRY(copy(entry.shell)),JS::default_attributes);
    return result.ptr();
}
JS::ThrowCompletionOr<int> priority_integer(JS::VM& vm,size_t index,double min,double max)
{
    auto value=vm.argument(index);
    if(!value.is_number())return vm.throw_completion<JS::TypeError>("Invalid internal priority argument"sv);
    auto number=value.as_double();
    if(!std::isfinite(number) || std::trunc(number)!=number || number<min || number>max)
        return vm.throw_completion<JS::TypeError>("Invalid internal priority range"sv);
    return static_cast<int>(number);
}
template<typename Read>
JS::ThrowCompletionOr<JS::Value> read_text(JS::VM& vm, char const* syscall, Read read, bool missing_is_undefined = false)
{
    std::vector<char> buffer(256);
    for (;;) {
        size_t size=buffer.size();
        int status=read(buffer.data(), &size);
        if(status==UV_ENOBUFS){buffer.resize(size+1);continue;}
        if(missing_is_undefined && status==UV_ENOENT)return JS::js_undefined();
        if(status)return system_error(vm,syscall,status);
        return text(vm,buffer.data());
    }
}
}
JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_os_host(JS::Realm& realm)
{
    auto& vm=realm.vm();
    uv_utsname_t information {};
    if(auto status=uv_os_uname(&information);status)return system_error(vm,"uv_os_uname",status);
    GC::Root host {JS::Object::create(realm,nullptr)};
    GC::Root info {JS::Object::create(realm,nullptr)};
    info->define_direct_property("type"_utf16,text(vm,information.sysname),JS::default_attributes);
    info->define_direct_property("release"_utf16,text(vm,information.release),JS::default_attributes);
    info->define_direct_property("version"_utf16,text(vm,information.version),JS::default_attributes);
    info->define_direct_property("machine"_utf16,text(vm,information.machine),JS::default_attributes);
    host->define_direct_property("constants"_utf16,TRY(create_os_constants(realm)),JS::default_attributes);
    host->define_direct_property("information"_utf16,info.ptr(),JS::default_attributes);
    host->define_direct_property("endianness"_utf16,text(vm,std::endian::native==std::endian::big?"BE":"LE"),JS::default_attributes);
    host->define_native_function(realm,"getPriority"_utf16,+[](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        auto pid=TRY(priority_integer(vm,0,-2147483648.0,2147483647.0));
        int priority=0;
        if(auto status=uv_os_getpriority(pid,&priority);status)return system_error(vm,"uv_os_getpriority",status);
        return JS::Value(priority);
    },1,JS::default_attributes);
    host->define_native_function(realm,"setPriority"_utf16,+[](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        auto pid=TRY(priority_integer(vm,0,-2147483648.0,2147483647.0));
        auto priority=TRY(priority_integer(vm,1,-20,19));
        if(auto status=uv_os_setpriority(pid,priority);status)return system_error(vm,"uv_os_setpriority",status);
        return JS::js_undefined();
    },2,JS::default_attributes);
    host->define_native_function(realm,"getenv"_utf16,+[](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!vm.argument(0).is_string())return vm.throw_completion<JS::TypeError>("Invalid internal environment key"sv);
        auto key=MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        return read_text(vm,"uv_os_getenv",[&](char* bytes,size_t* size){return uv_os_getenv(key.characters(),bytes,size);},true);
    },1,JS::default_attributes);
    host->define_native_function(realm,"query"_utf16,+[](JS::VM& vm)->JS::ThrowCompletionOr<JS::Value>{
        if(!vm.argument(0).is_string())return vm.throw_completion<JS::TypeError>("Invalid internal OS query"sv);
        auto name=MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        if(name=="networkInterfaces")return network_interfaces(vm);
        if(name=="cpus")return cpu_info(vm);
        if(name=="userInfo")return user_info(vm);
        if(name=="hostname")return read_text(vm,"uv_os_gethostname",uv_os_gethostname);
        if(name=="homedir"){
#ifdef PATH_MAX
            char buffer[PATH_MAX];
#else
            char buffer[32768];
#endif
            size_t size=sizeof(buffer);
            if(auto status=uv_os_homedir(buffer,&size);status)return system_error(vm,"uv_os_homedir",status);
            return text(vm,buffer);
        }
        if(name=="freemem")return JS::Value(static_cast<double>(uv_get_free_memory()));
        if(name=="totalmem")return JS::Value(static_cast<double>(uv_get_total_memory()));
        if(name=="availableParallelism")return JS::Value(uv_available_parallelism());
        if(name=="uptime"){
            double seconds=0;
            if(auto status=uv_uptime(&seconds);status)return system_error(vm,"uv_uptime",status);
            return JS::Value(seconds);
        }
        if(name=="loadavg"){
            double values[3] {};
            uv_loadavg(values);
            GC::Root array {TRY(JS::Array::create(vm.realm(),3))};
            for(u32 i=0;i<3;++i)TRY(array->create_data_property_or_throw(i,JS::Value(values[i])));
            return array.ptr();
        }
        return vm.throw_completion<JS::TypeError>("Unknown internal OS query"sv);
    },1,JS::default_attributes);
    return *host;
}
}
