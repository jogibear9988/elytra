#pragma once
#include "IsolatedWorld.h"
#include <LibJS/Runtime/NativeFunction.h>
namespace Web::DOM { class Document; }
namespace Firefly {
class FunctionBridge;
struct FunctionExport {
    NonnullRefPtr<FunctionBridge> owner;
    GC::Ref<JS::NativeFunction> function;
};
// One synchronous, data-only endpoint for one exact document/isolated world.
// Its native registry MUST revoke it before document/world teardown; a target
// global may otherwise retain the document through this object's explicit roots.
class FunctionBridge final : public RefCounted<FunctionBridge> {
public:
    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<FunctionExport> create(IsolatedWorld&, JS::FunctionObject&, Web::DOM::Document&);
    void revoke();
    bool is_revoked() const { return m_revoked; }
private:
    FunctionBridge(IsolatedWorld&, JS::FunctionObject&, Web::DOM::Document&);
    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&);
    RefPtr<IsolatedWorld> m_source;
    GC::Root<JS::FunctionObject> m_function;
    GC::Root<Web::DOM::Document> m_document;
    bool m_revoked { false };
    bool m_calling { false };
};
}
