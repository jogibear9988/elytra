#pragma once
#include <AK/Error.h>

namespace JS {
class VM;
}

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_principal_callbacks(JS::VM&, Web::DOM::Document&);
