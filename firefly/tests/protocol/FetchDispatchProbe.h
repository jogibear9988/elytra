/** Actual JavaScript fetch through the owned scheme-fetch seam and real native file jobs. */
#pragma once
#include <AK/Error.h>
#include <AK/StringView.h>

namespace JS {
class Realm;
}

namespace Web::DOM {
class Document;
}

ErrorOr<void> check_fetch_dispatch(JS::Realm&, Web::DOM::Document&, StringView authorized_fixture);
