#pragma once
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Realm.h>

namespace Mite {
JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_process(JS::VM&, JS::Realm&);
}
