/**
 * Owned WebContent executable entry. Reuse the upstream bootstrap/sandbox and install the
 * selected service only after the renderer VM is initialized.
 */

// Consume the owned native policy before upstream bootstrap/sandbox/connection
// setup. Rename only its entry and VM-initialization call inside the composition.
#include "RendererSession.h"
#include "native/protocol/OriginConfig.h"
#include "preload/Process.h"
#include "resources/Session.h"
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <LibWeb/Bindings/MainThreadVM.h>

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType);
}

ErrorOr<int> firefly_upstream_renderer_main(Main::Arguments);
#define ladybird_main firefly_upstream_renderer_main
#define initialize_main_thread_vm firefly_initialize_renderer_vm
#include <Services/WebContent/main.cpp>
#undef initialize_main_thread_vm
#undef ladybird_main

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType type)
{
    initialize_main_thread_vm(type);
    Firefly::RendererSession::initialize();
    Firefly::Resources::initialize();
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    TRY(Firefly::PreloadProcess::prepare());
    auto filtered = TRY(Firefly::Protocols::consume_origin_configuration(arguments.strings));
    // Read-only startup evidence while this internal protocol path is under
    // development. No page/preload command can set or replace this policy.
    if (auto configuration = Firefly::Protocols::configuration_for_helper();
        configuration.has_value())
        dbgln("FIREFLY_ORIGIN_POLICY_READY pid={} configuration={}", Core::System::getpid(),
            *configuration);
    auto result = firefly_upstream_renderer_main({ static_cast<int>(filtered.strings.size()),
        filtered.argv.data(), filtered.strings.span() });
    Firefly::Resources::shutdown();
    Firefly::RendererSession::shutdown();
    return result;
}
