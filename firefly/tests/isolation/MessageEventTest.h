#pragma once
#include <AK/Error.h>
namespace JS { class VM; }
namespace Web::DOM { class Document; }
ErrorOr<void> check_message_event_values(JS::VM&, Web::DOM::Document&);
