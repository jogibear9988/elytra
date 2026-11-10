/** Byte-preserving response-header validation, shared by both native control peers. */
#pragma once
#include <AK/JsonObject.h>

namespace Firefly::WebRequest {
ErrorOr<void> validate_headers(JsonObject const&);

ErrorOr<void> validate_decision(
    JsonObject const&, JsonObject const* original_headers, bool request_headers = false);
}
