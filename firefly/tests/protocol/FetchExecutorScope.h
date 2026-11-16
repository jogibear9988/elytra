/** Install the genuine owner-agent Fetch engine for standalone native probes. */
#pragma once

#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/Fetch/Engine/FetchExecutor.h>
#include <LibWeb/Fetch/Fetching/FetchEngine.h>
#include <LibWeb/HTML/Scripting/Agent.h>

namespace Firefly::Tests {

// Declare after the principal realm/document roots and before transport owners.
// Transport teardown then runs first, followed by executor shutdown while the
// owning agent/VM and its real network-activity observers still live.
class FetchExecutorScope final {
public:
    explicit FetchExecutorScope(JS::Realm& realm)
        : m_agent(*static_cast<Web::HTML::Agent*>(realm.vm().agent()))
    {
        VERIFY(!m_agent.fetch_executor);

        m_agent.fetch_executor = Web::Fetch::Fetching::create_inline_fetch_executor(
            m_agent.network_activity_observers);
    }

    ~FetchExecutorScope()
    {
        m_agent.fetch_executor->shutdown();

        m_agent.fetch_executor = nullptr;
    }

    FetchExecutorScope(FetchExecutorScope const&) = delete;
    FetchExecutorScope& operator=(FetchExecutorScope const&) = delete;

private:
    Web::HTML::Agent& m_agent;
};

}
