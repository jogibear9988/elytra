/** Dedicated workers inherit native creator authority, never a URL-derived capability. */
#include "Scope.h"
#include "Fetch.h"
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Weak.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/Page/Page.h>
#include <LibWebCommon/HTML/WorkerAgentTypes.h>
#include <map>
#include <mutex>
#include <utility>

namespace Firefly::Workers {
struct Scope::Impl {
    Descriptor descriptor;
    std::atomic<bool> revoked { false };
    std::shared_ptr<Impl> parent;
    std::shared_ptr<Impl> root;
    RefPtr<Core::WeakEventLoopReference> owner_loop;
    std::mutex mutex;
    RefPtr<Core::WeakEventLoopReference> worker_loop;
};

namespace {
    struct OwnerRecord {
        GC::Weak<Web::DOM::Document> document;
        Web::HTML::EnvironmentId environment;
        std::weak_ptr<Scope> scope;
    };

    struct Registry {
        AK::ThreadID owner { AK::ThreadID::current() };
        std::atomic<bool> enabled { false };
        Function<Optional<u64>(Web::DOM::Document&)> generation;
        // Cell metadata is exclusively constructed, read and destroyed on owner.
        std::map<u64, OwnerRecord> documents;
        // Shared records contain only immutable native metadata and atomic retirement.
        std::mutex mutex;
        std::map<u64, std::shared_ptr<Scope>> scopes;
    };

    Registry& registry()
    {
        static NeverDestroyed<Registry> value;
        return *value;
    }

    struct ThreadState {
        std::shared_ptr<Scope> scope;
        std::string environment;
    };

    ThreadState& thread_state()
    {
        static thread_local NeverDestroyed<ThreadState> value;
        return *value;
    }

    std::string environment_string(Web::HTML::EnvironmentId const& id)
    {
        auto const& value = id.to_string();
        return { value.bytes_as_string_view().characters_without_null_termination(),
            value.byte_count() };
    }
}

Scope::Scope(std::shared_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

Descriptor const& Scope::descriptor() const
{
    return m_impl->descriptor;
}

bool Scope::revoked() const
{
    if (m_impl->revoked.load(std::memory_order_acquire))
        return true;
    for (auto parent = m_impl->parent; parent; parent = parent->parent)
        if (parent->revoked.load(std::memory_order_acquire))
            return true;
    return false;
}

bool Scope::matches_environment(Web::HTML::EnvironmentId const& environment) const
{
    return thread_state().scope.get() == this && !revoked() && !thread_state().environment.empty()
        && thread_state().environment == environment_string(environment);
}

void Scope::revoke()
{
    if (m_impl->revoked.exchange(true, std::memory_order_acq_rel))
        return;
    RefPtr<Core::WeakEventLoopReference> reference;
    {
        std::lock_guard lock(m_impl->mutex);
        reference = m_impl->worker_loop;
    }

    if (!reference)
        return;
    auto loop = reference->take();
    if (!loop.is_alive())
        return;
    // Retirement intentionally bypasses the ordinary post_worker's revoked
    // guard. Only the actual still-attached agent may touch its own VM roots.
    loop->deferred_invoke([self = shared_from_this()] {
        if (thread_state().scope == self)
            WorkersFetch::retire();
    });
}

GC::Ptr<Web::DOM::Document> owner_document(Scope const& scope)
{
    auto& state = registry();
    VERIFY(state.owner.is_current_thread());
    if (!state.enabled.load(std::memory_order_acquire) || scope.revoked())
        return nullptr;
    auto root_token = scope.m_impl->root ? scope.m_impl->root->descriptor.token : scope.token();
    auto found = state.documents.find(root_token);
    if (found == state.documents.end())
        return nullptr;
    auto document = found->second.document.ptr();
    auto const& descriptor = scope.descriptor();
    auto navigable = document ? document->navigable() : nullptr;
    if (!document || !document->is_fully_active() || !navigable || navigable->has_been_destroyed()
        || !navigable->active_document_is(*document)
        || document->id()
            != Web::HTML::CrossProcessId { descriptor.document_namespace,
                descriptor.document_local }
        || navigable->id()
            != Web::HTML::CrossProcessId { descriptor.navigable_namespace,
                descriptor.navigable_local }
        || document->page().client().id().value() != descriptor.page
        || document->relevant_settings_object().id != found->second.environment || !state.generation
        || state.generation(*document) != descriptor.generation)
        return nullptr;
    return document;
}

bool Scope::post_owner(std::function<void()> task)
{
    if (revoked() || !m_impl->owner_loop)
        return false;
    auto loop = m_impl->owner_loop->take();
    if (!loop.is_alive()) {
        revoke();
        return false;
    }

    loop->deferred_invoke([self = shared_from_this(), task = std::move(task)] {
        if (!owner_document(*self)) {
            self->revoke();
            return;
        }

        task();
    });
    return true;
}

bool Scope::post_worker(std::function<void()> task)
{
    if (revoked())
        return false;
    RefPtr<Core::WeakEventLoopReference> reference;
    {
        std::lock_guard lock(m_impl->mutex);
        reference = m_impl->worker_loop;
    }

    if (!reference)
        return false;
    auto loop = reference->take();
    if (!loop.is_alive()) {
        revoke();
        return false;
    }

    loop->deferred_invoke([self = shared_from_this(), task = std::move(task)] {
        if (!self->revoked() && thread_state().scope == self)
            task();
    });
    return true;
}

Admission::Admission() = default;

Admission::Admission(std::shared_ptr<Scope> scope)
    : m_scope(std::move(scope))
{
}

Admission::Admission(Admission&& other)
    : m_scope(std::move(other.m_scope))
    , m_committed(std::exchange(other.m_committed, true))
{
}

Admission::~Admission()
{
    if (m_scope && !m_committed)
        m_scope->revoke();
}

void Admission::commit()
{
    m_committed = true;
}

void install_generation_resolver(Function<Optional<u64>(Web::DOM::Document&)> resolver)
{
    auto& state = registry();
    VERIFY(state.owner.is_current_thread());
    shutdown();
    state.generation = move(resolver);
    state.enabled.store(!!state.generation, std::memory_order_release);
}

ErrorOr<Admission> admit(Web::Page& page, Web::HTML::WorkerAgentStartRequest const& request)
{
    auto& state = registry();
    if (!state.enabled.load(std::memory_order_acquire))
        return Admission {};
    if (!request.owner_token || request.outside_settings.id.is_empty()
        || request.agent_type != Web::HTML::AgentType::DedicatedWorker)
        return Error::from_string_literal("Worker has no supported native owner scope");
    auto impl = std::make_shared<Scope::Impl>();
    auto& descriptor = impl->descriptor;
    descriptor.token = request.owner_token;
    descriptor.creator_environment = environment_string(request.outside_settings.id);
    GC::Ptr<Web::DOM::Document> document;
    if (thread_state().scope) {
        if (thread_state().scope->revoked() || thread_state().environment.empty()
            || descriptor.creator_environment != thread_state().environment)
            return Error::from_string_literal(
                "Nested worker creator scope is retired or mismatched");
        impl->parent = thread_state().scope->m_impl;
        impl->root = impl->parent->root ? impl->parent->root : impl->parent;
        descriptor = impl->parent->descriptor;
        descriptor.token = request.owner_token;
        descriptor.parent = thread_state().scope->token();
        descriptor.creator_environment = thread_state().environment;
        impl->owner_loop = impl->parent->owner_loop;
    } else {
        if (!state.owner.is_current_thread())
            return Admission {}; // Unmanaged worker descendants retain upstream behavior.
        sweep();
        for (auto const& navigable : page.hosted_navigables()) {
            auto candidate = navigable->active_document();
            if (candidate && candidate->is_fully_active()
                && candidate->relevant_settings_object().id == request.outside_settings.id) {
                if (document)
                    return Error::from_string_literal("Ambiguous native worker creator");
                document = candidate;
            }
        }

        if (!document || !document->navigable() || document->navigable()->has_been_destroyed()
            || !document->navigable()->active_document_is(*document))
            return Error::from_string_literal("Worker creator document is not current");
        auto generation = state.generation ? state.generation(*document) : Optional<u64> {};
        if (!generation.has_value() || !*generation)
            return Admission {}; // No protocol grant is not itself a worker-creation failure.
        descriptor.document_namespace = document->id().namespace_id;
        descriptor.document_local = document->id().local_id;
        descriptor.navigable_namespace = document->navigable()->id().namespace_id;
        descriptor.navigable_local = document->navigable()->id().local_id;
        descriptor.page = page.client().id().value();
        descriptor.generation = *generation;
        impl->owner_loop = Core::EventLoop::current_weak();
    }

    auto scope = std::shared_ptr<Scope>(new Scope(std::move(impl)));
    {
        std::lock_guard lock(state.mutex);
        if (state.scopes.size() >= 1024 || state.scopes.contains(request.owner_token))
            return Error::from_string_literal("Worker native scope budget or identity exceeded");
        size_t same_document = 0;
        for (auto const& entry : state.scopes) {
            auto const& other = entry.second->descriptor();
            if (other.document_namespace == descriptor.document_namespace
                && other.document_local == descriptor.document_local)
                ++same_document;
        }

        if (same_document >= 128)
            return Error::from_string_literal("Worker native document scope budget exceeded");
        state.scopes.emplace(request.owner_token, scope);
    }

    if (document)
        state.documents.emplace(request.owner_token,
            OwnerRecord {
                GC::Weak<Web::DOM::Document> { document }, request.outside_settings.id, scope });
    return Admission { std::move(scope) };
}

AgentScope::AgentScope(u64 owner_token)
{
    auto& state = registry();
    {
        std::lock_guard lock(state.mutex);
        auto found = state.scopes.find(owner_token);
        if (found != state.scopes.end())
            m_scope = found->second;
    }

    VERIFY(!thread_state().scope);
    thread_state().scope = m_scope;
    if (m_scope) {
        std::lock_guard lock(m_scope->m_impl->mutex);
        m_scope->m_impl->worker_loop = Core::EventLoop::current_weak();
    }
}

AgentScope::~AgentScope()
{
    if (m_scope) {
        m_scope->revoke();
        {
            std::lock_guard lock(m_scope->m_impl->mutex);
            m_scope->m_impl->worker_loop = nullptr;
        }

        auto& state = registry();
        std::lock_guard lock(state.mutex);
        state.scopes.erase(m_scope->token());
    }

    std::string {}.swap(thread_state().environment);
    thread_state().scope.reset();
}

std::shared_ptr<Scope> current()
{
    return thread_state().scope;
}

bool bind_environment(Web::HTML::EnvironmentId const& environment)
{
    if (!registry().enabled.load(std::memory_order_acquire))
        return true;
    if (!thread_state().scope)
        return true; // Interception remains fail-closed when its policy is active.
    if (thread_state().scope->revoked() || environment.is_empty()
        || !thread_state().environment.empty())
        return false;
    thread_state().environment = environment_string(environment);
    return true;
}

void retire_current()
{
    if (thread_state().scope)
        thread_state().scope->revoke();
}

void sweep()
{
    auto& state = registry();
    VERIFY(state.owner.is_current_thread());
    for (auto iterator = state.documents.begin(); iterator != state.documents.end();) {
        auto scope = iterator->second.scope.lock();
        if (!scope || !owner_document(*scope)) {
            if (scope)
                scope->revoke();
            iterator = state.documents.erase(iterator);
        } else {
            ++iterator;
        }
    }

    std::lock_guard lock(state.mutex);
    std::erase_if(state.scopes, [](auto const& entry) {
        if (!entry.second->revoked())
            return false;
        // A revoked ancestor must actively notify every descendant agent too.
        entry.second->revoke();
        return true;
    });
}

void shutdown()
{
    auto& state = registry();
    VERIFY(state.owner.is_current_thread());
    state.enabled.store(false, std::memory_order_release);
    {
        std::lock_guard lock(state.mutex);
        for (auto const& entry : state.scopes)
            entry.second->revoke();
        state.scopes.clear();
    }

    state.documents.clear();
    state.generation = {};
}
}
