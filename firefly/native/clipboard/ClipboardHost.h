/** Register the private synchronous OS boundary behind the asynchronous public API. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Firefly {
void install_clipboard_host(JS::Realm&, JS::Object&);
}
