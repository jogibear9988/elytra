#pragma once
#include <AK/Error.h>
#include <LibJS/Forward.h>

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_file_origins(JS::VM&, Web::DOM::Document&);
