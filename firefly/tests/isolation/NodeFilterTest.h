#pragma once
#include <AK/Error.h>
namespace JS { class VM; }
namespace Web::DOM { class Document; }
ErrorOr<void> check_node_filters(JS::VM&, Web::DOM::Document&);
