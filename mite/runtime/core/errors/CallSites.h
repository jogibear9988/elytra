/** Real source locations exposed to an application's Error.prepareStackTrace callback. */
#pragma once
#include <AK/Optional.h>
#include <AK/Utf16String.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/SourceRange.h>

namespace JS {
class Object;
class Realm;
class VM;
}

namespace Mite {
/** Copy capture-time metadata; never retain a borrowed ExecutionContext. */
struct CapturedSite {
    Utf16String name;
    Optional<JS::SourceRange> source;
    Utf16String display;
    bool native { false };
};

JS::ThrowCompletionOr<GC::Ref<JS::Object>> install_call_sites(JS::Realm&);

JS::ThrowCompletionOr<GC::Ref<JS::Object>> make_call_site(
    JS::VM&, JS::Object& factory, CapturedSite const&);
}
