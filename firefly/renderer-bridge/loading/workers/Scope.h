/** Native creator authority for dedicated-worker fetches; no cell crosses an agent thread. */
#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/Optional.h>
#include <AK/Types.h>
#include <LibGC/Ptr.h>
#include <LibWeb/Export.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace Web {
class Page;

namespace DOM {
    class Document;
}

namespace HTML {
    struct WorkerAgentStartRequest;
    class EnvironmentId;
}
}

namespace Firefly::Workers {
struct Descriptor {
    u64 token {}, parent {};
    u64 document_namespace {}, document_local {};
    u64 navigable_namespace {}, navigable_local {};
    u64 page {}, generation {};
    std::string creator_environment;
};

class WEB_API Scope final : public std::enable_shared_from_this<Scope> {
public:
    Descriptor const& descriptor() const;

    u64 token() const
    {
        return descriptor().token;
    }

    u64 generation() const
    {
        return descriptor().generation;
    }

    bool revoked() const;

    bool matches_environment(Web::HTML::EnvironmentId const&) const;

    void revoke();
    // Tasks must contain only native values or numeric keys into thread-local registries.
    // They are dropped if the endpoint/creator was retired before delivery.
    bool post_owner(std::function<void()>);

    bool post_worker(std::function<void()>);

private:
    friend class AgentScope;
    friend class Admission;
    friend ErrorOr<class Admission> admit(Web::Page&, Web::HTML::WorkerAgentStartRequest const&);

    friend GC::Ptr<Web::DOM::Document> owner_document(Scope const&);

    struct Impl;
    explicit Scope(std::shared_ptr<Impl>);

    std::shared_ptr<Impl> m_impl;
};

class WEB_API Admission final {
public:
    Admission();

    Admission(Admission&&);

    ~Admission();

    void commit();

private:
    friend ErrorOr<Admission> admit(Web::Page&, Web::HTML::WorkerAgentStartRequest const&);

    explicit Admission(std::shared_ptr<Scope>);

    std::shared_ptr<Scope> m_scope;
    bool m_committed {};
};

// Created on the new worker thread, before its agent starts; destroyed after shutdown.
class WEB_API AgentScope final {
public:
    explicit AgentScope(u64 owner_token);

    ~AgentScope();

    AgentScope(AgentScope const&) = delete;
    AgentScope& operator=(AgentScope const&) = delete;

private:
    std::shared_ptr<Scope> m_scope;
};

WEB_API void install_generation_resolver(Function<Optional<u64>(Web::DOM::Document&)>);

WEB_API ErrorOr<Admission> admit(Web::Page&, Web::HTML::WorkerAgentStartRequest const&);

WEB_API std::shared_ptr<Scope> current();
// Owner-thread only. Resolves/revalidates actual creator identity and binding generation.
WEB_API GC::Ptr<Web::DOM::Document> owner_document(Scope const&);

WEB_API bool bind_environment(Web::HTML::EnvironmentId const&);

WEB_API void retire_current();

WEB_API void sweep();

WEB_API void shutdown();
}
