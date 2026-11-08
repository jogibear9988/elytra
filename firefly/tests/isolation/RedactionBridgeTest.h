#pragma once
#include <AK/Error.h>
#include <LibJS/Forward.h>

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_redaction_bridge(JS::VM&, Web::DOM::Document&);
