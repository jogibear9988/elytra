/** Share callback proxy identity across all exported API functions; reclaim dead cache entries. */
#include "CallbackBridge.h"
#include <LibGC/WeakInlines.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
CallbackRegistry::CallbackRegistry(IsolatedWorld& source, Web::DOM::Document& doc)
    : world(source)
    , document(doc)
{
}

ErrorOr<NonnullRefPtr<CallbackRegistry>> CallbackRegistry::create(
    IsolatedWorld& source, Web::DOM::Document& document)
{
    if (source.is_detached() || !document.is_fully_active())
        return Error::from_string_literal("Callback registry requires a live world/document");
    auto owner = adopt_ref(*new CallbackRegistry(source, document));
    source.add_detach_hook([weak = owner->make_weak_ptr()] {
        if (auto owner = weak.strong_ref())
            owner->revoke();
    });
    return owner;
}

bool CallbackRegistry::active() const
{
    return !m_revoked && world && !world->is_detached() && document && document->is_fully_active();
}

void CallbackRegistry::sweep()
{
    m_entries.remove_all_matching([](auto const& entry) {
        auto owner = entry.owner.strong_ref();
        if (!owner)
            return true;
        // Weak JS pointers clear during marking, but GC::Function capture boxes
        // release their native owners only during incremental sweep. Do not keep
        // a dead proxy's budget or principal roots until that later destruction.
        // An in-flight Promise still needs this entry for document revocation.
        if (!entry.proxy.ptr() && !owner->has_pending()) {
            owner->revoke();
            return true;
        }

        return false;
    });
}

size_t CallbackRegistry::size()
{
    sweep();
    return m_entries.size();
}

size_t CallbackRegistry::proxy_count()
{
    sweep();
    size_t count = 0;
    for (auto const& entry : m_entries)
        if (entry.proxy.ptr())
            ++count;
    return count;
}

ErrorOr<CallbackExport> CallbackRegistry::proxy(JS::FunctionObject& function)
{
    if (!active() || function.realm() != &document->window()->principal_realm())
        return Error::from_string_literal("Callback must belong to this exact principal realm");
    sweep();
    for (auto const& entry : m_entries)
        if (entry.original.ptr().ptr() == &function && entry.proxy.ptr())
            return CallbackExport { *entry.proxy.ptr(), {} };
    if (m_entries.size() >= 64)
        return Error::from_string_literal("Callback proxy budget exceeded");
    auto exported = TRY(CallbackEndpoint::create(*this, function));
    m_entries.append({ GC::Weak { function }, GC::Weak { *exported.function },
        exported.created->make_weak_ptr() });
    return exported;
}

void CallbackRegistry::discard(CallbackEndpoint& owner)
{
    m_entries.remove_all_matching([&](auto const& entry) { return entry.owner.ptr() == &owner; });
    owner.revoke();
}

void CallbackRegistry::revoke()
{
    if (m_revoked)
        return;
    m_revoked = true;
    auto entries = move(m_entries);
    for (auto& entry : entries)
        if (auto owner = entry.owner.strong_ref())
            owner->revoke();
    document = nullptr;
    world.clear();
}
}
