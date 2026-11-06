/**
 * Converts byte ranges and text without assuming typed-array element values. Decoder state is
 * owned per instance; shared buffers use native snapshots and VM-facing results remain
 * realm-local.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_encoding_host(JS::Realm&);
}
