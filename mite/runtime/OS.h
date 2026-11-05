#pragma once
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/Completion.h>
namespace Mite {
JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_os_host(JS::Realm&);
}
