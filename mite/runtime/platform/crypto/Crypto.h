/**
 * Provides OS entropy and native crypto host factories. Worker requests own native bytes only;
 * completion objects and callbacks stay on the VM owner thread.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_hmac_host(JS::Realm&);

GC::Ref<JS::Object> create_hash_host(JS::Realm&);
GC::Ref<JS::Object> create_x509_host(JS::Realm&);

GC::Ref<JS::Object> create_crypto_host(JS::VM&, JS::Realm&, EventLoop&);
}
