#include "OS.h"
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <uv.h>
#ifndef _WIN32
#    include <dlfcn.h>
#endif

namespace Mite {
namespace {
void constant(JS::Object& target,char const* name,int value)
{
    target.define_direct_property(Utf16String::from_utf8(StringView(name,strlen(name))),JS::Value(value),JS::Attribute::Enumerable);
}
}
JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_os_constants(JS::Realm& realm)
{
    GC::Root result {JS::Object::create(realm,nullptr)};
    constant(*result,"UV_UDP_REUSEADDR",UV_UDP_REUSEADDR);
    GC::Root dlopen {JS::Object::create(realm,nullptr)};
    GC::Root errors {JS::Object::create(realm,nullptr)};
    GC::Root signals {JS::Object::create(realm,nullptr)};
    GC::Root priority {JS::Object::create(realm,nullptr)};
    // Public Node-compatible names; values and availability come from native
    // headers. There is no Node source inclusion or build-time Node query.
#ifdef RTLD_LAZY
    constant(*dlopen,"RTLD_LAZY",RTLD_LAZY);
#endif
#ifdef RTLD_NOW
    constant(*dlopen,"RTLD_NOW",RTLD_NOW);
#endif
#ifdef RTLD_GLOBAL
    constant(*dlopen,"RTLD_GLOBAL",RTLD_GLOBAL);
#endif
#ifdef RTLD_LOCAL
    constant(*dlopen,"RTLD_LOCAL",RTLD_LOCAL);
#endif
#ifdef RTLD_DEEPBIND
    constant(*dlopen,"RTLD_DEEPBIND",RTLD_DEEPBIND);
#endif
#ifdef E2BIG
    constant(*errors,"E2BIG",E2BIG);
#endif
#ifdef EACCES
    constant(*errors,"EACCES",EACCES);
#endif
#ifdef EADDRINUSE
    constant(*errors,"EADDRINUSE",EADDRINUSE);
#endif
#ifdef EADDRNOTAVAIL
    constant(*errors,"EADDRNOTAVAIL",EADDRNOTAVAIL);
#endif
#ifdef EAFNOSUPPORT
    constant(*errors,"EAFNOSUPPORT",EAFNOSUPPORT);
#endif
#ifdef EAGAIN
    constant(*errors,"EAGAIN",EAGAIN);
#endif
#ifdef EALREADY
    constant(*errors,"EALREADY",EALREADY);
#endif
#ifdef EBADF
    constant(*errors,"EBADF",EBADF);
#endif
#ifdef EBADMSG
    constant(*errors,"EBADMSG",EBADMSG);
#endif
#ifdef EBUSY
    constant(*errors,"EBUSY",EBUSY);
#endif
#ifdef ECANCELED
    constant(*errors,"ECANCELED",ECANCELED);
#endif
#ifdef ECHILD
    constant(*errors,"ECHILD",ECHILD);
#endif
#ifdef ECONNABORTED
    constant(*errors,"ECONNABORTED",ECONNABORTED);
#endif
#ifdef ECONNREFUSED
    constant(*errors,"ECONNREFUSED",ECONNREFUSED);
#endif
#ifdef ECONNRESET
    constant(*errors,"ECONNRESET",ECONNRESET);
#endif
#ifdef EDEADLK
    constant(*errors,"EDEADLK",EDEADLK);
#endif
#ifdef EDESTADDRREQ
    constant(*errors,"EDESTADDRREQ",EDESTADDRREQ);
#endif
#ifdef EDOM
    constant(*errors,"EDOM",EDOM);
#endif
#ifdef EDQUOT
    constant(*errors,"EDQUOT",EDQUOT);
#endif
#ifdef EEXIST
    constant(*errors,"EEXIST",EEXIST);
#endif
#ifdef EFAULT
    constant(*errors,"EFAULT",EFAULT);
#endif
#ifdef EFBIG
    constant(*errors,"EFBIG",EFBIG);
#endif
#ifdef EHOSTUNREACH
    constant(*errors,"EHOSTUNREACH",EHOSTUNREACH);
#endif
#ifdef EIDRM
    constant(*errors,"EIDRM",EIDRM);
#endif
#ifdef EILSEQ
    constant(*errors,"EILSEQ",EILSEQ);
#endif
#ifdef EINPROGRESS
    constant(*errors,"EINPROGRESS",EINPROGRESS);
#endif
#ifdef EINTR
    constant(*errors,"EINTR",EINTR);
#endif
#ifdef EINVAL
    constant(*errors,"EINVAL",EINVAL);
#endif
#ifdef EIO
    constant(*errors,"EIO",EIO);
#endif
#ifdef EISCONN
    constant(*errors,"EISCONN",EISCONN);
#endif
#ifdef EISDIR
    constant(*errors,"EISDIR",EISDIR);
#endif
#ifdef ELOOP
    constant(*errors,"ELOOP",ELOOP);
#endif
#ifdef EMFILE
    constant(*errors,"EMFILE",EMFILE);
#endif
#ifdef EMLINK
    constant(*errors,"EMLINK",EMLINK);
#endif
#ifdef EMSGSIZE
    constant(*errors,"EMSGSIZE",EMSGSIZE);
#endif
#ifdef EMULTIHOP
    constant(*errors,"EMULTIHOP",EMULTIHOP);
#endif
#ifdef ENAMETOOLONG
    constant(*errors,"ENAMETOOLONG",ENAMETOOLONG);
#endif
#ifdef ENETDOWN
    constant(*errors,"ENETDOWN",ENETDOWN);
#endif
#ifdef ENETRESET
    constant(*errors,"ENETRESET",ENETRESET);
#endif
#ifdef ENETUNREACH
    constant(*errors,"ENETUNREACH",ENETUNREACH);
#endif
#ifdef ENFILE
    constant(*errors,"ENFILE",ENFILE);
#endif
#ifdef ENOBUFS
    constant(*errors,"ENOBUFS",ENOBUFS);
#endif
#ifdef ENODATA
    constant(*errors,"ENODATA",ENODATA);
#endif
#ifdef ENODEV
    constant(*errors,"ENODEV",ENODEV);
#endif
#ifdef ENOENT
    constant(*errors,"ENOENT",ENOENT);
#endif
#ifdef ENOEXEC
    constant(*errors,"ENOEXEC",ENOEXEC);
#endif
#ifdef ENOLCK
    constant(*errors,"ENOLCK",ENOLCK);
#endif
#ifdef ENOLINK
    constant(*errors,"ENOLINK",ENOLINK);
#endif
#ifdef ENOMEM
    constant(*errors,"ENOMEM",ENOMEM);
#endif
#ifdef ENOMSG
    constant(*errors,"ENOMSG",ENOMSG);
#endif
#ifdef ENOPROTOOPT
    constant(*errors,"ENOPROTOOPT",ENOPROTOOPT);
#endif
#ifdef ENOSPC
    constant(*errors,"ENOSPC",ENOSPC);
#endif
#ifdef ENOSR
    constant(*errors,"ENOSR",ENOSR);
#endif
#ifdef ENOSTR
    constant(*errors,"ENOSTR",ENOSTR);
#endif
#ifdef ENOSYS
    constant(*errors,"ENOSYS",ENOSYS);
#endif
#ifdef ENOTCONN
    constant(*errors,"ENOTCONN",ENOTCONN);
#endif
#ifdef ENOTDIR
    constant(*errors,"ENOTDIR",ENOTDIR);
#endif
#ifdef ENOTEMPTY
    constant(*errors,"ENOTEMPTY",ENOTEMPTY);
#endif
#ifdef ENOTSOCK
    constant(*errors,"ENOTSOCK",ENOTSOCK);
#endif
#ifdef ENOTSUP
    constant(*errors,"ENOTSUP",ENOTSUP);
#endif
#ifdef ENOTTY
    constant(*errors,"ENOTTY",ENOTTY);
#endif
#ifdef ENXIO
    constant(*errors,"ENXIO",ENXIO);
#endif
#ifdef EOPNOTSUPP
    constant(*errors,"EOPNOTSUPP",EOPNOTSUPP);
#endif
#ifdef EOVERFLOW
    constant(*errors,"EOVERFLOW",EOVERFLOW);
#endif
#ifdef EPERM
    constant(*errors,"EPERM",EPERM);
#endif
#ifdef EPIPE
    constant(*errors,"EPIPE",EPIPE);
#endif
#ifdef EPROTO
    constant(*errors,"EPROTO",EPROTO);
#endif
#ifdef EPROTONOSUPPORT
    constant(*errors,"EPROTONOSUPPORT",EPROTONOSUPPORT);
#endif
#ifdef EPROTOTYPE
    constant(*errors,"EPROTOTYPE",EPROTOTYPE);
#endif
#ifdef ERANGE
    constant(*errors,"ERANGE",ERANGE);
#endif
#ifdef EROFS
    constant(*errors,"EROFS",EROFS);
#endif
#ifdef ESPIPE
    constant(*errors,"ESPIPE",ESPIPE);
#endif
#ifdef ESRCH
    constant(*errors,"ESRCH",ESRCH);
#endif
#ifdef ESTALE
    constant(*errors,"ESTALE",ESTALE);
#endif
#ifdef ETIME
    constant(*errors,"ETIME",ETIME);
#endif
#ifdef ETIMEDOUT
    constant(*errors,"ETIMEDOUT",ETIMEDOUT);
#endif
#ifdef ETXTBSY
    constant(*errors,"ETXTBSY",ETXTBSY);
#endif
#ifdef EWOULDBLOCK
    constant(*errors,"EWOULDBLOCK",EWOULDBLOCK);
#endif
#ifdef EXDEV
    constant(*errors,"EXDEV",EXDEV);
#endif
#ifdef WSAEINTR
    constant(*errors,"WSAEINTR",WSAEINTR);
#endif
#ifdef WSAEBADF
    constant(*errors,"WSAEBADF",WSAEBADF);
#endif
#ifdef WSAEACCES
    constant(*errors,"WSAEACCES",WSAEACCES);
#endif
#ifdef WSAEFAULT
    constant(*errors,"WSAEFAULT",WSAEFAULT);
#endif
#ifdef WSAEINVAL
    constant(*errors,"WSAEINVAL",WSAEINVAL);
#endif
#ifdef WSAEMFILE
    constant(*errors,"WSAEMFILE",WSAEMFILE);
#endif
#ifdef WSAEWOULDBLOCK
    constant(*errors,"WSAEWOULDBLOCK",WSAEWOULDBLOCK);
#endif
#ifdef WSAEINPROGRESS
    constant(*errors,"WSAEINPROGRESS",WSAEINPROGRESS);
#endif
#ifdef WSAEALREADY
    constant(*errors,"WSAEALREADY",WSAEALREADY);
#endif
#ifdef WSAENOTSOCK
    constant(*errors,"WSAENOTSOCK",WSAENOTSOCK);
#endif
#ifdef WSAEDESTADDRREQ
    constant(*errors,"WSAEDESTADDRREQ",WSAEDESTADDRREQ);
#endif
#ifdef WSAEMSGSIZE
    constant(*errors,"WSAEMSGSIZE",WSAEMSGSIZE);
#endif
#ifdef WSAEPROTOTYPE
    constant(*errors,"WSAEPROTOTYPE",WSAEPROTOTYPE);
#endif
#ifdef WSAENOPROTOOPT
    constant(*errors,"WSAENOPROTOOPT",WSAENOPROTOOPT);
#endif
#ifdef WSAEPROTONOSUPPORT
    constant(*errors,"WSAEPROTONOSUPPORT",WSAEPROTONOSUPPORT);
#endif
#ifdef WSAESOCKTNOSUPPORT
    constant(*errors,"WSAESOCKTNOSUPPORT",WSAESOCKTNOSUPPORT);
#endif
#ifdef WSAEOPNOTSUPP
    constant(*errors,"WSAEOPNOTSUPP",WSAEOPNOTSUPP);
#endif
#ifdef WSAEPFNOSUPPORT
    constant(*errors,"WSAEPFNOSUPPORT",WSAEPFNOSUPPORT);
#endif
#ifdef WSAEAFNOSUPPORT
    constant(*errors,"WSAEAFNOSUPPORT",WSAEAFNOSUPPORT);
#endif
#ifdef WSAEADDRINUSE
    constant(*errors,"WSAEADDRINUSE",WSAEADDRINUSE);
#endif
#ifdef WSAEADDRNOTAVAIL
    constant(*errors,"WSAEADDRNOTAVAIL",WSAEADDRNOTAVAIL);
#endif
#ifdef WSAENETDOWN
    constant(*errors,"WSAENETDOWN",WSAENETDOWN);
#endif
#ifdef WSAENETUNREACH
    constant(*errors,"WSAENETUNREACH",WSAENETUNREACH);
#endif
#ifdef WSAENETRESET
    constant(*errors,"WSAENETRESET",WSAENETRESET);
#endif
#ifdef WSAECONNABORTED
    constant(*errors,"WSAECONNABORTED",WSAECONNABORTED);
#endif
#ifdef WSAECONNRESET
    constant(*errors,"WSAECONNRESET",WSAECONNRESET);
#endif
#ifdef WSAENOBUFS
    constant(*errors,"WSAENOBUFS",WSAENOBUFS);
#endif
#ifdef WSAEISCONN
    constant(*errors,"WSAEISCONN",WSAEISCONN);
#endif
#ifdef WSAENOTCONN
    constant(*errors,"WSAENOTCONN",WSAENOTCONN);
#endif
#ifdef WSAESHUTDOWN
    constant(*errors,"WSAESHUTDOWN",WSAESHUTDOWN);
#endif
#ifdef WSAETOOMANYREFS
    constant(*errors,"WSAETOOMANYREFS",WSAETOOMANYREFS);
#endif
#ifdef WSAETIMEDOUT
    constant(*errors,"WSAETIMEDOUT",WSAETIMEDOUT);
#endif
#ifdef WSAECONNREFUSED
    constant(*errors,"WSAECONNREFUSED",WSAECONNREFUSED);
#endif
#ifdef WSAELOOP
    constant(*errors,"WSAELOOP",WSAELOOP);
#endif
#ifdef WSAENAMETOOLONG
    constant(*errors,"WSAENAMETOOLONG",WSAENAMETOOLONG);
#endif
#ifdef WSAEHOSTDOWN
    constant(*errors,"WSAEHOSTDOWN",WSAEHOSTDOWN);
#endif
#ifdef WSAEHOSTUNREACH
    constant(*errors,"WSAEHOSTUNREACH",WSAEHOSTUNREACH);
#endif
#ifdef WSAENOTEMPTY
    constant(*errors,"WSAENOTEMPTY",WSAENOTEMPTY);
#endif
#ifdef WSAEPROCLIM
    constant(*errors,"WSAEPROCLIM",WSAEPROCLIM);
#endif
#ifdef WSAEUSERS
    constant(*errors,"WSAEUSERS",WSAEUSERS);
#endif
#ifdef WSAEDQUOT
    constant(*errors,"WSAEDQUOT",WSAEDQUOT);
#endif
#ifdef WSAESTALE
    constant(*errors,"WSAESTALE",WSAESTALE);
#endif
#ifdef WSAEREMOTE
    constant(*errors,"WSAEREMOTE",WSAEREMOTE);
#endif
#ifdef WSASYSNOTREADY
    constant(*errors,"WSASYSNOTREADY",WSASYSNOTREADY);
#endif
#ifdef WSAVERNOTSUPPORTED
    constant(*errors,"WSAVERNOTSUPPORTED",WSAVERNOTSUPPORTED);
#endif
#ifdef WSANOTINITIALISED
    constant(*errors,"WSANOTINITIALISED",WSANOTINITIALISED);
#endif
#ifdef WSAEDISCON
    constant(*errors,"WSAEDISCON",WSAEDISCON);
#endif
#ifdef WSAENOMORE
    constant(*errors,"WSAENOMORE",WSAENOMORE);
#endif
#ifdef WSAECANCELLED
    constant(*errors,"WSAECANCELLED",WSAECANCELLED);
#endif
#ifdef WSAEINVALIDPROCTABLE
    constant(*errors,"WSAEINVALIDPROCTABLE",WSAEINVALIDPROCTABLE);
#endif
#ifdef WSAEINVALIDPROVIDER
    constant(*errors,"WSAEINVALIDPROVIDER",WSAEINVALIDPROVIDER);
#endif
#ifdef WSAEPROVIDERFAILEDINIT
    constant(*errors,"WSAEPROVIDERFAILEDINIT",WSAEPROVIDERFAILEDINIT);
#endif
#ifdef WSASYSCALLFAILURE
    constant(*errors,"WSASYSCALLFAILURE",WSASYSCALLFAILURE);
#endif
#ifdef WSASERVICE_NOT_FOUND
    constant(*errors,"WSASERVICE_NOT_FOUND",WSASERVICE_NOT_FOUND);
#endif
#ifdef WSATYPE_NOT_FOUND
    constant(*errors,"WSATYPE_NOT_FOUND",WSATYPE_NOT_FOUND);
#endif
#ifdef WSA_E_NO_MORE
    constant(*errors,"WSA_E_NO_MORE",WSA_E_NO_MORE);
#endif
#ifdef WSA_E_CANCELLED
    constant(*errors,"WSA_E_CANCELLED",WSA_E_CANCELLED);
#endif
#ifdef WSAEREFUSED
    constant(*errors,"WSAEREFUSED",WSAEREFUSED);
#endif
#ifdef SIGHUP
    constant(*signals,"SIGHUP",SIGHUP);
#endif
#ifdef SIGINT
    constant(*signals,"SIGINT",SIGINT);
#endif
#ifdef SIGQUIT
    constant(*signals,"SIGQUIT",SIGQUIT);
#endif
#ifdef SIGILL
    constant(*signals,"SIGILL",SIGILL);
#endif
#ifdef SIGTRAP
    constant(*signals,"SIGTRAP",SIGTRAP);
#endif
#ifdef SIGABRT
    constant(*signals,"SIGABRT",SIGABRT);
#endif
#ifdef SIGIOT
    constant(*signals,"SIGIOT",SIGIOT);
#endif
#ifdef SIGBUS
    constant(*signals,"SIGBUS",SIGBUS);
#endif
#ifdef SIGFPE
    constant(*signals,"SIGFPE",SIGFPE);
#endif
#ifdef SIGKILL
    constant(*signals,"SIGKILL",SIGKILL);
#endif
#ifdef SIGUSR1
    constant(*signals,"SIGUSR1",SIGUSR1);
#endif
#ifdef SIGSEGV
    constant(*signals,"SIGSEGV",SIGSEGV);
#endif
#ifdef SIGUSR2
    constant(*signals,"SIGUSR2",SIGUSR2);
#endif
#ifdef SIGPIPE
    constant(*signals,"SIGPIPE",SIGPIPE);
#endif
#ifdef SIGALRM
    constant(*signals,"SIGALRM",SIGALRM);
#endif
#ifdef SIGTERM
    constant(*signals,"SIGTERM",SIGTERM);
#endif
#ifdef SIGCHLD
    constant(*signals,"SIGCHLD",SIGCHLD);
#endif
#ifdef SIGSTKFLT
    constant(*signals,"SIGSTKFLT",SIGSTKFLT);
#endif
#ifdef SIGCONT
    constant(*signals,"SIGCONT",SIGCONT);
#endif
#ifdef SIGSTOP
    constant(*signals,"SIGSTOP",SIGSTOP);
#endif
#ifdef SIGTSTP
    constant(*signals,"SIGTSTP",SIGTSTP);
#endif
#ifdef SIGBREAK
    constant(*signals,"SIGBREAK",SIGBREAK);
#endif
#ifdef SIGTTIN
    constant(*signals,"SIGTTIN",SIGTTIN);
#endif
#ifdef SIGTTOU
    constant(*signals,"SIGTTOU",SIGTTOU);
#endif
#ifdef SIGURG
    constant(*signals,"SIGURG",SIGURG);
#endif
#ifdef SIGXCPU
    constant(*signals,"SIGXCPU",SIGXCPU);
#endif
#ifdef SIGXFSZ
    constant(*signals,"SIGXFSZ",SIGXFSZ);
#endif
#ifdef SIGVTALRM
    constant(*signals,"SIGVTALRM",SIGVTALRM);
#endif
#ifdef SIGPROF
    constant(*signals,"SIGPROF",SIGPROF);
#endif
#ifdef SIGWINCH
    constant(*signals,"SIGWINCH",SIGWINCH);
#endif
#ifdef SIGIO
    constant(*signals,"SIGIO",SIGIO);
#endif
#ifdef SIGPOLL
    constant(*signals,"SIGPOLL",SIGPOLL);
#endif
#ifdef SIGLOST
    constant(*signals,"SIGLOST",SIGLOST);
#endif
#ifdef SIGPWR
    constant(*signals,"SIGPWR",SIGPWR);
#endif
#ifdef SIGINFO
    constant(*signals,"SIGINFO",SIGINFO);
#endif
#ifdef SIGSYS
    constant(*signals,"SIGSYS",SIGSYS);
#endif
#ifdef SIGUNUSED
    constant(*signals,"SIGUNUSED",SIGUNUSED);
#endif
#ifdef UV_PRIORITY_LOW
    constant(*priority,"PRIORITY_LOW",UV_PRIORITY_LOW);
#endif
#ifdef UV_PRIORITY_BELOW_NORMAL
    constant(*priority,"PRIORITY_BELOW_NORMAL",UV_PRIORITY_BELOW_NORMAL);
#endif
#ifdef UV_PRIORITY_NORMAL
    constant(*priority,"PRIORITY_NORMAL",UV_PRIORITY_NORMAL);
#endif
#ifdef UV_PRIORITY_ABOVE_NORMAL
    constant(*priority,"PRIORITY_ABOVE_NORMAL",UV_PRIORITY_ABOVE_NORMAL);
#endif
#ifdef UV_PRIORITY_HIGH
    constant(*priority,"PRIORITY_HIGH",UV_PRIORITY_HIGH);
#endif
#ifdef UV_PRIORITY_HIGHEST
    constant(*priority,"PRIORITY_HIGHEST",UV_PRIORITY_HIGHEST);
#endif
    // Native freezing also holds if user code replaces Object.freeze before
    // importing os. Other groups remain extensible, as in the reference.
    VERIFY(TRY(signals->set_integrity_level(JS::Object::IntegrityLevel::Frozen)));
    result->define_direct_property("dlopen"_utf16,dlopen.ptr(),JS::default_attributes);
    result->define_direct_property("errno"_utf16,errors.ptr(),JS::default_attributes);
    result->define_direct_property("signals"_utf16,signals.ptr(),JS::default_attributes);
    result->define_direct_property("priority"_utf16,priority.ptr(),JS::default_attributes);
    return *result;
}
}
